//
// Created by Dylan Haye
// Corrected Multi-Resolution BSpline (ITK 5.4.5)
//

#include <itkVersion.h>
#include <iostream>
#include <vector>

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkResampleImageFilter.h>

#include <itkBSplineTransform.h>
#include <itkBSplineTransformInitializer.h>
#include <itkBSplineTransformParametersAdaptor.h>

#include <itkLBFGSBOptimizerv4.h>
#include <itkLBFGSOptimizerv4.h>
#include <itkImageRegistrationMethodv4.h>
#include <itkMattesMutualInformationImageToImageMetricv4.h>
#include <itkLinearInterpolateImageFunction.h>

#include <itkTransformToDisplacementFieldFilter.h>
#include <itkDisplacementFieldJacobianDeterminantFilter.h>
#include <itkStatisticsImageFilter.h>

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: "
                  << argv[0]
                  << " <fixed> <moving> <output>\n";
        return EXIT_FAILURE;
    }

    std::cout << "ITK Version: "
              << itk::Version::GetITKVersion()
              << std::endl;

    using ImageType = itk::Image<float, 3>;
    using ReaderType = itk::ImageFileReader<ImageType>;

    auto fixedReader  = ReaderType::New();
    auto movingReader = ReaderType::New();

    fixedReader->SetFileName(argv[1]);
    movingReader->SetFileName(argv[2]);

    fixedReader->Update();
    movingReader->Update();

    auto fixedImage  = fixedReader->GetOutput();
    auto movingImage = movingReader->GetOutput();

    // =====================================================
    // BSpline Transform
    // =====================================================

    using TransformType = itk::BSplineTransform<double, 3, 3>;
    auto transform = TransformType::New();

    using InitializerType =
        itk::BSplineTransformInitializer<TransformType, ImageType>;

    auto initializer = InitializerType::New();
    initializer->SetTransform(transform);
    initializer->SetImage(fixedImage);

    // COARSE INITIAL GRID
    TransformType::MeshSizeType meshSize;
    meshSize.Fill(4);  // coarse start
    initializer->SetTransformDomainMeshSize(meshSize);
    initializer->InitializeTransform();

    // =====================================================
    // Metric
    // =====================================================

    using MetricType =
        itk::MattesMutualInformationImageToImageMetricv4<ImageType, ImageType>;

    auto metric = MetricType::New();
    metric->SetNumberOfHistogramBins(50);
    metric->SetUseMovingImageGradientFilter(false);
    metric->SetUseFixedImageGradientFilter(false);

    // =====================================================
    // Optimizer
    // =====================================================

    //using OptimizerType = itk::LBFGSBOptimizerv4;
    using OptimizerType = itk::LBFGSOptimizerv4;
    auto optimizer = OptimizerType::New();

    optimizer->SetGradientConvergenceTolerance(1e-5);
    // optimizer->SetNumberOfIterations(100);
    // optimizer->SetMaximumNumberOfFunctionEvaluations(500);
    optimizer->SetNumberOfIterations(30);
    optimizer->SetMaximumNumberOfFunctionEvaluations(100);

    // IMPORTANT:
    // Do NOT pre-size bounds based on parameter count.
    // ITK will resize parameters between levels.

    // =====================================================
    // Registration
    // =====================================================

    using RegistrationType =
        itk::ImageRegistrationMethodv4<ImageType, ImageType>;

    auto registration = RegistrationType::New();

    registration->SetFixedImage(fixedImage);
    registration->SetMovingImage(movingImage);

    registration->SetMetric(metric);
    registration->SetOptimizer(optimizer);
    registration->SetInitialTransform(transform);

    registration->InPlaceOn();

    // ==============================
    // MULTI-RESOLUTION SETTINGS
    // ==============================

    // const unsigned int numberOfLevels = 3;
    const unsigned int numberOfLevels = 2;
    registration->SetNumberOfLevels(numberOfLevels);

    // Shrink factors + smoothing
    RegistrationType::ShrinkFactorsArrayType shrinkFactorsPerLevel;
    RegistrationType::SmoothingSigmasArrayType smoothingSigmasPerLevel;

    shrinkFactorsPerLevel.SetSize(numberOfLevels);
    smoothingSigmasPerLevel.SetSize(numberOfLevels);

    for (unsigned int level = 0; level < numberOfLevels; ++level)
    {
        shrinkFactorsPerLevel[level] = 4 >> level;   // 4,2
        smoothingSigmasPerLevel[level] = 2 - level;  // 2,1
    }

    registration->SetShrinkFactorsPerLevel(shrinkFactorsPerLevel);
    registration->SetSmoothingSigmasPerLevel(smoothingSigmasPerLevel);

    // =====================================================
    // BSpline Adaptors (CRITICAL PART)
    // =====================================================

    using TransformAdaptorType =
        itk::BSplineTransformParametersAdaptor<TransformType>;

    RegistrationType::TransformParametersAdaptorsContainerType adaptors;

    for (unsigned int level = 0; level < numberOfLevels; ++level)
    {
        auto adaptor = TransformAdaptorType::New();
        adaptor->SetTransform(transform);

        TransformType::MeshSizeType levelMesh;
        // levelMesh.Fill(4 * std::pow(2, level));  // refine grid
        levelMesh.Fill(3 + level);  // refine grid

        adaptor->SetRequiredTransformDomainMeshSize(levelMesh);
        adaptor->SetRequiredTransformDomainOrigin(
            transform->GetTransformDomainOrigin());
        adaptor->SetRequiredTransformDomainDirection(
            transform->GetTransformDomainDirection());
        adaptor->SetRequiredTransformDomainPhysicalDimensions(
            transform->GetTransformDomainPhysicalDimensions());

        adaptors.push_back(adaptor.GetPointer());
    }

    registration->SetTransformParametersAdaptorsPerLevel(adaptors);

    // =====================================================
    // RUN REGISTRATION
    // =====================================================

    try
    {
        registration->Update();
    }
    catch (itk::ExceptionObject & err)
    {
        std::cerr << "Registration failed:\n"
                  << err << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Multi-resolution deformable registration completed.\n";

    // =====================================================
    // RESAMPLE RESULT
    // =====================================================

    using ResampleFilterType =
        itk::ResampleImageFilter<ImageType, ImageType>;

    auto resampler = ResampleFilterType::New();
    resampler->SetInput(movingImage);
    resampler->SetTransform(transform);
    resampler->SetReferenceImage(fixedImage);
    resampler->UseReferenceImageOn();
    resampler->SetInterpolator(
        itk::LinearInterpolateImageFunction<ImageType, double>::New());

    using WriterType = itk::ImageFileWriter<ImageType>;
    auto writer = WriterType::New();
    writer->SetFileName(argv[3]);
    writer->SetInput(resampler->GetOutput());
    writer->Update();

    std::cout << "Output written.\n";

    return EXIT_SUCCESS;
}