//
// Created by Dylan Haye on 04/01/2026.
//
#include <iostream>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIterator.h>
#include <cmath>

int main (int argc, char *argv[]){
  if(argc < 3){
    std::cerr << "Usage: " << argv[0]
              << " <input_resampled.nii.gz> <outpput_normalized.nii.gz>"
              << std::endl;
        return EXIT_FAILURE;
  }

  using ImageType = itk::Image<float, 3>;
  using ReaderType = itk::ImageFileReader<ImageType>;
  using WriterType = itk::ImageFileWriter<ImageType>;

  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(argv[1]);
  reader->Update();

  ImageType::Pointer image = reader->GetOutput();

  itk::ImageRegionIterator<ImageType> it(image, image->GetLargestPossibleRegion());
  double sum = 0.0;
  size_t count = 0;

  for (it.GoToBegin(); !it.IsAtEnd(); ++it){
    sum += it.Get();
    count++;
  }

  double mean = sum / count;

  double variance = 0.0;
  for (it.GoToBegin(); !it.IsAtEnd(); ++it){
    double diff = it.Get() - mean;
    variance += diff * diff;
  }

  double stddev = std::sqrt(variance / count);

  for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
    it.Set((it.Get() - mean) / stddev);
  }
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(argv[2]);
  writer->SetInput(image);
  writer->Update();

  std::cout << "Intensity normalization complete." << std::endl;
  std::cout << "Mean: " << mean << " StdDev: " << stddev << std::endl;
}