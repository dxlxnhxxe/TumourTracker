//
// Created by Dylan Haye on 06/01/2026.
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

// Metric: how we measure alignment quality
#include <itkMattesMutualInformationImageToImageMetricv4.h>

// Optimizer: how we search for best alignment
#include <itkRegularStepGradientDescentOptimizerv4.h>

// Transform: what kind of motion is allowed
#include <itkEuler3DTransform.h>

// --------------------
// Resampling (apply transform)
// --------------------
#include <itkResampleImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>

int main (int argc, char *argv[]){
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0]
                           << " <fixed_T0.nii> <moving_T1.nii> <output_rigid.nii>"
                           <<std::endl;
    return EXIT_FAILURE;
    }

    //Define image type (3D MRI, float values)
    using ImageType = itk::Image<float, 3>;
    using ReaderType = itk::ImageFileReader<ImageType>;

    auto fixedReader = ReaderType::New();
    auto movingReader = ReaderType::New();

    fixedReader->SetFileName(argv[1]);
    movingReader->SetFileName(argv[2]);

    try {
      fixedReader->Update();
      movingReader->Update();
    }
    catch (itk::ExceptionObject & err) {
        std::cerr << "ExceptionObject caught: " << err << std::endl;
        std::cerr << err <<std::endl;
        return EXIT_FAILURE;
    }

    //Transform: Rigid motion in 3D
    //Euler3DTransform allows:
    // - 3 roations (X,Y,Z)
    // - 3 translations (X,Y,Z)
    using TransformType = itk::Euler3DTransform<double>;
    auto transform = TransformType::New();

    //Start with identity = no movement
    transform->SetIdentity();

    //How do we measure similarity ?
    //Mattes Mutual Information is robus for MRI and works well when intensitiies differ
    using MetricType = itk::MattesMutualInformationImageToImageMetricv4<ImageType, ImageType>;

    auto metric = MetricType::New();
    metric->SetNumberOfHistogramBins(32);

    //Optimizer: How do we find best parapmeters?
    using OptimizerType = itk::RegularStepGradientDescentOptimizerv4<double>;
    auto optimizer = OptimizerType::New();

    //Large initial steps, then smaller refinements
    optimizer->SetLearningRate(4.0);
    optimizer->SetMinimumStepLength(0.01);
    optimizer->SetNumberOfIterations(200);

    //Registration method
    using RegistrationType = itk::ImageRegistrationMethodv4<ImageType, ImageType>;
    auto registration = RegistrationType::New();

    //Fixed = reference image (T0)
    //Moving = image we transform (T1)
    registration->SetFixedImage(fixedReader->GetOutput());
    registration->SetMovingImage(movingReader->GetOutput());

    registration->SetMetric(metric);
    registration->SetOptimizer(optimizer);

    //Provide initial guess (idenitty transform)
    registration->SetInitialTransform(transform);
    //Modify transform directly
    registration->InPlaceOn();

    //Try running the registration
    try {
      registration->Update();
    }
    catch (itk::ExceptionObject & err) {
      std::cerr << "Registration failed: " << err << std::endl;
      return EXIT_FAILURE;
    }

    //Apply the resulting transform
    using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;
    auto resampler = ResampleFilterType::New();

    //Resample moving image using the optimized transform
    resampler->SetInput(movingReader->GetOutput());
    resampler->SetTransform(transform);

    //Match the fixed image grind exactly
    resampler->SetReferenceImage(fixedReader->GetOutput());
    resampler->UseReferenceImageOn();

    //Linear interpolation is standard for MRI
    resampler->SetInterpolator(itk::LinearInterpolateImageFunction<ImageType, double>::New());

    //Write the registered image to disk
    using WriterType =itk::ImageFileWriter<ImageType>;
    auto writer = WriterType::New();

    writer->SetFileName(argv[3]);
    writer->SetInput(resampler->GetOutput());
    writer->Update();

    std::cout << "Rigid registration complete." << std::endl;
    return EXIT_SUCCESS;
}