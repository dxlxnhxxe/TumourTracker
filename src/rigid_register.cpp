//
// Rigid registration between two 3D MRI volumes (NIfTI)
// Fixed image = T0
// Moving image = T1
//

#include <iostream>

// --------------------
// Core ITK image types
// --------------------
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>

// --------------------
// Registration components
// --------------------
#include <itkImageRegistrationMethodv4.h>
#include <itkMattesMutualInformationImageToImageMetricv4.h>
#include <itkRegularStepGradientDescentOptimizerv4.h>
#include <itkEuler3DTransform.h>

// --------------------
// Resampling
// --------------------
#include <itkResampleImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <fixed_T0.nii> <moving_T1.nii> <output_rigid.nii>\n";
        return EXIT_FAILURE;
    }

    // --------------------------------------------------
    // Image type (3D MRI stored as float)
    // --------------------------------------------------
    using ImageType = itk::Image<float, 3>;
    using ReaderType = itk::ImageFileReader<ImageType>;

    auto fixedReader  = ReaderType::New();
    auto movingReader = ReaderType::New();

    fixedReader->SetFileName(argv[1]);
    movingReader->SetFileName(argv[2]);

    try
    {
        fixedReader->Update();
        movingReader->Update();
    }
    catch (itk::ExceptionObject &err)
    {
        std::cerr << "Error reading images:\n" << err << std::endl;
        return EXIT_FAILURE;
    }

    ImageType::Pointer fixedImage  = fixedReader->GetOutput();
    ImageType::Pointer movingImage = movingReader->GetOutput();

    // --------------------------------------------------
    // Rigid transform (3 rotations + 3 translations)
    // --------------------------------------------------
    using TransformType = itk::Euler3DTransform<double>;
    auto transform = TransformType::New();
    transform->SetIdentity();

    // --------------------------------------------------
    // IMPORTANT: Set center of rotation to image center
    // --------------------------------------------------
    ImageType::RegionType region = fixedImage->GetLargestPossibleRegion();
    ImageType::SizeType   size   = region.GetSize();
    ImageType::SpacingType spacing = fixedImage->GetSpacing();
    ImageType::PointType  origin  = fixedImage->GetOrigin();

    TransformType::InputPointType center;
    for (unsigned int i = 0; i < 3; ++i)
    {
        center[i] = origin[i] + spacing[i] * size[i] / 2.0;
    }
    transform->SetCenter(center);

    // --------------------------------------------------
    // Metric: Mutual Information (robust for MRI)
    // --------------------------------------------------
    using MetricType =
        itk::MattesMutualInformationImageToImageMetricv4<ImageType, ImageType>;

    auto metric = MetricType::New();
    metric->SetNumberOfHistogramBins(50);
    metric->SetUseFixedImageGradientFilter(false);
    metric->SetUseMovingImageGradientFilter(false);

    // --------------------------------------------------
    // Optimizer
    // --------------------------------------------------
    using OptimizerType =
        itk::RegularStepGradientDescentOptimizerv4<double>;

    auto optimizer = OptimizerType::New();
    optimizer->SetLearningRate(4.0);
    optimizer->SetMinimumStepLength(0.01);
    optimizer->SetNumberOfIterations(200);

    // --------------------------------------------------
    // IMPORTANT: Parameter scaling
    // Rotations (radians) vs translations (mm)
    // --------------------------------------------------
    OptimizerType::ScalesType scales(transform->GetNumberOfParameters());
    scales[0] = 1.0;          // rot X
    scales[1] = 1.0;          // rot Y
    scales[2] = 1.0;          // rot Z
    scales[3] = 1.0 / 1000.0; // trans X
    scales[4] = 1.0 / 1000.0; // trans Y
    scales[5] = 1.0 / 1000.0; // trans Z
    optimizer->SetScales(scales);

    // --------------------------------------------------
    // Registration setup
    // --------------------------------------------------
    using RegistrationType =
        itk::ImageRegistrationMethodv4<ImageType, ImageType>;

    auto registration = RegistrationType::New();

    registration->SetFixedImage(fixedImage);
    registration->SetMovingImage(movingImage);
    registration->SetMetric(metric);
    registration->SetOptimizer(optimizer);
    registration->SetInitialTransform(transform);
    registration->InPlaceOn();

    try
    {
        registration->Update();
    }
    catch (itk::ExceptionObject &err)
    {
        std::cerr << "Registration failed:\n" << err << std::endl;
        return EXIT_FAILURE;
    }

    // --------------------------------------------------
    // Resample moving image using optimized transform
    // --------------------------------------------------
    using ResampleFilterType =
        itk::ResampleImageFilter<ImageType, ImageType>;

    auto resampler = ResampleFilterType::New();
    resampler->SetInput(movingImage);
    resampler->SetTransform(transform);
    resampler->SetReferenceImage(fixedImage);
    resampler->UseReferenceImageOn();
    resampler->SetInterpolator(
        itk::LinearInterpolateImageFunction<ImageType, double>::New());

    // --------------------------------------------------
    // Write output
    // --------------------------------------------------
    using WriterType = itk::ImageFileWriter<ImageType>;
    auto writer = WriterType::New();
    writer->SetFileName(argv[3]);
    writer->SetInput(resampler->GetOutput());

    try
    {
        writer->Update();
    }
    catch (itk::ExceptionObject &err)
    {
        std::cerr << "Error writing output:\n" << err << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Rigid registration completed successfully." << std::endl;
    return EXIT_SUCCESS;
}