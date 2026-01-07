#include <iostream>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkResampleImageFilter.h>
#include <itkIdentityTransform.h>
#include <itkLinearInterpolateImageFunction.h>

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr <<"Usage: " << argv[0] << " <input_nifti.nii.gz> <output_nifti.nii.gz" << std::endl;
        return EXIT_FAILURE;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];

    using ImageType =itk::Image<float, 3>; //you can now write ImageType instead of itk::Image<float,3> everywhere
    using ReaderType = itk::ImageFileReader<ImageType>;
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(inputFile);

    try {
        reader->Update();
    } catch (itk::ExceptionObject &error) {
        std::cerr <<"Error reading image: " << error << std::endl;
        return EXIT_FAILURE;
    }
    ImageType::Pointer inputImage = reader->GetOutput();

    using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;
    ResampleFilterType::Pointer resampler = ResampleFilterType::New();

    using TransformType = itk::IdentityTransform<double, 3>;
    TransformType::Pointer transform =TransformType::New();

    using InterpolatorType = itk::LinearInterpolateImageFunction<ImageType, double>;
    InterpolatorType::Pointer interpolator = InterpolatorType::New();

    ImageType::SpacingType newSpacing;
    newSpacing.Fill(1.0);

    ImageType::SizeType inputSize = inputImage->GetLargestPossibleRegion().GetSize();
    ImageType::SpacingType inputSpacing = inputImage->GetSpacing();
    ImageType::SizeType newSize;
    for (unsigned int i =0; i < 3; i++) {
        newSize[i] = static_cast<unsigned int>(inputSize[i] * (inputSpacing[i] / newSpacing[i]));
    }

    resampler->SetInput(inputImage);
    resampler->SetTransform(transform);
    resampler->SetInterpolator(interpolator);
    resampler->SetOutputSpacing(newSpacing);
    resampler->SetSize(newSize);
    resampler->SetOutputOrigin(inputImage->GetOrigin());
    resampler->SetOutputDirection(inputImage->GetDirection());

    using WriterType =itk::ImageFileWriter<ImageType>;
    WriterType::Pointer writer = WriterType::New();
    writer->SetFileName(outputFile);
    writer->SetInput(resampler->GetOutput());

    try {
        writer->Update();
    } catch(itk::ExceptionObject &error) {
        std::cerr <<"Error writing image: " << error << std::endl;
        return EXIT_FAILURE;
    }
    std::cout <<"Resampling complete!" << std::endl;
    std::cout << "New spacing: "
              << newSpacing[0] << " "
              << newSpacing[1] << " "
              << newSpacing[2] << std::endl;
    std::cout << "New size: "
              << newSize[0] << " "
              << newSize[1] << " "
              << newSize[2] << std::endl;

    return EXIT_SUCCESS;
}
