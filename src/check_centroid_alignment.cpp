//
// Foreground centroid comparison for registration sanity check
//

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkImageMomentsCalculator.h>
#include <iostream>

using ImageType = itk::Image<float, 3>;
using ReaderType = itk::ImageFileReader<ImageType>;
using MomentsType = itk::ImageMomentsCalculator<ImageType>;
using PointType = itk::Point<double, 3>;

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <fixed.nii> <registered.nii>\n";
        return EXIT_FAILURE;
    }

    auto fixedReader = ReaderType::New();
    auto regReader   = ReaderType::New();

    fixedReader->SetFileName(argv[1]);
    regReader->SetFileName(argv[2]);

    fixedReader->Update();
    regReader->Update();

    //Threshold to remove background (simple & robust)
    using ThreshType =
        itk::BinaryThresholdImageFilter<ImageType, ImageType>;

    auto threshFixed = ThreshType::New();
    threshFixed->SetInput(fixedReader->GetOutput());
    threshFixed->SetLowerThreshold(1.0);
    threshFixed->SetUpperThreshold(1e9);
    threshFixed->SetInsideValue(1.0);
    threshFixed->SetOutsideValue(0.0);
    threshFixed->Update();

    auto threshReg = ThreshType::New();
    threshReg->SetInput(regReader->GetOutput());
    threshReg->SetLowerThreshold(1.0);
    threshReg->SetUpperThreshold(1e9);
    threshReg->SetInsideValue(1.0);
    threshReg->SetOutsideValue(0.0);
    threshReg->Update();

    // Compute moments
    auto fixedMoments = MomentsType::New();
    fixedMoments->SetImage(threshFixed->GetOutput());
    fixedMoments->Compute();

    auto regMoments = MomentsType::New();
    regMoments->SetImage(threshReg->GetOutput());
    regMoments->Compute();

    PointType c1 = fixedMoments->GetCenterOfGravity();
    PointType c2 = regMoments->GetCenterOfGravity();

    std::cout << "Fixed centroid:      " << c1 << std::endl;
    std::cout << "Registered centroid: " << c2 << std::endl;
    std::cout << "Distance (mm): "
              << c1.EuclideanDistanceTo(c2)
              << std::endl;

    return EXIT_SUCCESS;
}