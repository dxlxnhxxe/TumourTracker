//
// Created by Dylan Haye on 08/01/2026.
//

#include <iostream>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkResampleImageFilter.h>
#include <itkBSplineTransform.h>
#include <itkLBFGSBOptimizerv4.h>
#include <itkImageRegistrationMethodv4.h>
#include <itkMattesMutualInformationImageToImageMetricv4.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkBSplineTransformInitializer.h>
#include <itkTransformToDisplacementFieldFilter.h>
#include <itkDisplacementFieldJacobianDeterminantFilter.h>
#include <itkStatisticsImageFilter.h>

//deal with arguments/command line
int main (int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage:" << argv[0]
                              << " <fixed_T0> <moving_T1> <output_deformed.nii>\n>";
        return EXIT_FAILURE;
    }

    //typdef the itkImage
    using ImageType = itk::Image<float, 3>;
    using ReaderType = itk::ImageFileReader<ImageType>;

    //instanciate 2 new readertypes
    auto fixedReader = ReaderType::New();
    auto movingReader = ReaderType::New();

    //set them as arguments 1 and 2
    fixedReader->SetFileName(argv[1]);
    movingReader->SetFileName(argv[2]);

    //trigger the reading process using the Update() method within a try block
    try {
        fixedReader->Update();
        movingReader->Update();
    } catch (const itk::ExceptionObject &err) {
        std::cerr << "Error reading images: " <<err << std::endl;
        return EXIT_FAILURE;
    }

    //Pass the output of the reader directly into the inputs of the writer (ImageType::Pointer) using GetOutput() method
    //and name them fixedImage and movingImage
    ImageType::Pointer fixedImage = fixedReader->GetOutput();
    ImageType::Pointer movingImage = movingReader->GetOutput();

    //-
    //B-spline deformable setup
    //-

    //Typedef Bspline (itk::BSplineTransform<TScalar, NDimensions, SplineOrder> as TransformType
    using TransformType = itk::BSplineTransform<double, 3, 3>;
    // create a new instance of TransformType
    auto transform = TransformType::New();

    // The BSplineTransformInitializer helps define the physical domain
    // (size, origin, spacing) of the B-spline grid based on an image
    using InitializerType = itk::BSplineTransformInitializer<TransformType, ImageType>;

    // Create a new initializer instance
    InitializerType::Pointer initializer = InitializerType::New();

    // Tell the initializer which transform we are initializing
    initializer->SetTransform(transform);

    // Tell the initializer which image defines the physical space
    // (we always use the fixed image as reference)
    initializer->SetImage(fixedImage);


    // Define how coarse the B-spline grid is
    // This is not voxel size — this is control point spacing
    // Small number = coarse deformation (safe, prevents overfitting)
    // 5 control points per dimension
    TransformType::MeshSizeType meshSize;
    meshSize.Fill(5);

    // Apply the mesh size to the initializer
    initializer->SetTransformDomainMeshSize(meshSize);

    // Actually initialize the transform domain
    initializer->InitializeTransform();

    //-
    // Metric (how similarity between images is measured)
    //-

    // Mutual Information is robust for MRI and multi-timepoint data
    using MetricType = itk::MattesMutualInformationImageToImageMetricv4<ImageType, ImageType>;

    // Create the metric
    auto metric = MetricType::New();

    // Number of histogram bins used to estimate probability distributions
    metric->SetNumberOfHistogramBins(50);

    // Disable gradient filters (faster, more stable for MRI)
    metric->SetUseMovingImageGradientFilter(false);
    metric->SetUseFixedImageGradientFilter(false);

    //-
    // Optimizer (how parameters are updated)
    //-

    // LBFGSB is suited for large parameter spaces
    using OptimizerType = itk::LBFGSBOptimizerv4;

    // Create optimizer
    auto optimizer = OptimizerType::New();


    // Stop when gradient change is small
    optimizer->SetGradientConvergenceTolerance(1e-4);

    // Maximum number of optimization iterations
    optimizer->SetNumberOfIterations(200);

    // Safety limit on cost function evaluations
    optimizer->SetMaximumNumberOfFunctionEvaluations(500);

    const unsigned int numberOfParameters = transform->GetNumberOfParameters();

    //Bound selection array:
    //0 = parameter is unbounded
    OptimizerType::BoundSelectionType boundSelect(numberOfParameters);
    boundSelect.Fill(0);

    OptimizerType::BoundValueType lowerBound(numberOfParameters);
    OptimizerType::BoundValueType upperBound(numberOfParameters);
    lowerBound.Fill(0);
    upperBound.Fill(0);

    //Assign bounds to optimizer
    optimizer->SetBoundSelection(boundSelect);
    optimizer->SetLowerBound(lowerBound);
    optimizer->SetUpperBound(upperBound);

    //-
    // Registration framework
    //-

    // ImageRegistrationMethodv4 orchestrates the process
    using RegistrationType = itk::ImageRegistrationMethodv4<ImageType, ImageType>;

    // Create registration object
    auto registration = RegistrationType::New();


    // Assign fixed and moving images
    registration->SetFixedImage(fixedImage);
    registration->SetMovingImage(movingImage);

    // Assign metric and optimizer
    registration->SetMetric(metric);
    registration->SetOptimizer(optimizer);

    // Use our initialized B-spline transform
    registration->SetInitialTransform(transform);

    // Allow the transform to be modified in-place
    registration->InPlaceOn();

    // Run the registration inside a try/catch
    try {
        registration->Update();
    } catch (itk::ExceptionObject &error) {
        std::cerr << "Deformable registration failed:\n"
                  << error << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Deformable registration completed successfully." << std::endl;

    //
    // Resample moving image using the optimized transform
    //

    // ResampleImageFilter applies the deformation to the moving image
    using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;

    // Create resampler
    auto resampler = ResampleFilterType::New();

    // Input is the original moving image
    resampler->SetInput(movingImage);

    // Apply the optimized B-spline transform
    resampler->SetTransform(transform);

    // Use fixed image for spacing, origin, direction, size
    resampler->SetReferenceImage(fixedImage);
    resampler->UseReferenceImageOn();

    // Linear interpolation is smooth and fast
    resampler->SetInterpolator(itk::LinearInterpolateImageFunction<ImageType, double>::New());

    //-
    // Write output image
    //-

    using WriterType = itk::ImageFileWriter<ImageType>;
    auto writer = WriterType::New();

    // Output filename comes from argv[3]
    writer->SetFileName(argv[3]);


    // Write the resampled image
    writer->SetInput(resampler->GetOutput());

    try {
        writer->Update();
    } catch (itk::ExceptionObject &error) {
        std::cerr << "Error writing output image:\n"
                  <<error << std::endl;
        return EXIT_FAILURE;
    }

    //
    // Jacobian determinant sanity check
    //


    // A displacement field stores per-voxel motion vectors
    using DisplacementFieldType = itk::Image<itk::Vector<double, 3>, 3>;

    // Convert transform to displacement field
    using TransformToFieldFilterType = itk::TransformToDisplacementFieldFilter<DisplacementFieldType, double>;

    auto fieldFilter = TransformToFieldFilterType::New();
    fieldFilter->SetTransform(transform);
    fieldFilter->SetReferenceImage(fixedImage);
    fieldFilter->UseReferenceImageOn();
    fieldFilter->Update();

    // Compute Jacobian determinant of displacement field
    using JacobianFilterType = itk::DisplacementFieldJacobianDeterminantFilter<DisplacementFieldType, double>;

    auto jacobianFilter = JacobianFilterType::New();
    jacobianFilter->SetInput(fieldFilter->GetOutput());

    //Define spatial domain for Jacobian image
    // jacobianFilter->SetReferenceImage(fixedImage);
    // jacobianFilter->UseReferenceImageOn();

    //Account for physical voxel spacing
    jacobianFilter->SetUseImageSpacing(true);

    //Run Computation
    jacobianFilter->Update();

    // Statistics must match the Jacobian image type (double)
    using JacobianImageType = itk::Image<double, 3>;
    using StatsFilterType = itk::StatisticsImageFilter<JacobianImageType>;

    auto statsFilter = StatsFilterType::New();
    statsFilter->SetInput(jacobianFilter->GetOutput());
    statsFilter->Update();

    // Extract min and max Jacobian values
    double minJac = statsFilter->GetMinimum();
    double maxJac = statsFilter->GetMaximum();


    std::cout <<"JacobianDeterminant range: [" << minJac << ", " << maxJac << "]" <<std::endl;

    // Non-positive Jacobian means folding (physically invalid deformation)
    if (minJac <=0.0) {
        std::cout << "WARNING: Non-Positive Jacobian detected." << std::endl;
    }

    return EXIT_SUCCESS;
}