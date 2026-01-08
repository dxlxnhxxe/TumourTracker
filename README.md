# TumourTracker  
**Longitudinal Tumour Evaluation from Multi-Timepoint MRI (NIfTI)**

TumourTracker is a medical imaging project focused on **longitudinal tumour tracking and quantitative evaluation** using multi-timepoint MRI data.  
The goal is to move beyond static segmentation and towards **physically consistent tumour tracking across time**.

This project is under active development, extensible, and open to collaboration.

---

## Context

In clinical oncology, radiologists often compare tumour appearance across multiple scans (weeks or months apart). This process is:

- Time-consuming  
- Subjective  
- Difficult to reproduce  

TumourTracker aims to provide:

- Physically meaningful longitudinal alignment  
- Objective tumour progression metrics  
- Clear visual and quantitative evidence of growth or regression  

---

## Current Scope

The project currently focuses on **brain tumour MRI** using **NIfTI (`.nii`) data**, with an emphasis on correctness in **physical space** rather than cosmetic re-orientation.

### Implemented / In Progress

- ✅ NIfTI-based image I/O (ITK)  
- ✅ Isotropic resampling (1 × 1 × 1 mm)  
- ✅ Intensity normalization (MRI-appropriate)  
- ✅ Rigid (6-DOF) registration between timepoints  
- 🚧 Registration validation & metric refinement  
- 🚧 Deformable (B-spline) registration  
- 🚧 Tumour segmentation & temporal correspondence  

---

## Long-Term Goals

- Longitudinal tumour tracking across multiple timepoints  
- Quantitative growth metrics:
  - Volume change  
  - Growth rate  
  - Boundary displacement  
  - Shape evolution analysis  
- Radiologist-grade visualization  
- Research-ready evaluation:
  - Dice coefficient  
  - Hausdorff distance  
  - Target Registration Error (TRE)  

---

## Data

This repository **does not contain imaging data**.

The project is developed using publicly available datasets from:

- **The Cancer Imaging Archive (TCIA)**  
- **CFB-GBM**: Pre- and post-treatment MRI of glioblastoma patients  
- DOI: `10.7937/v9pn-2f72`  

Dataset page:  
https://www.cancerimagingarchive.net/collection/cfb-gbm/

Users are expected to download datasets independently and place them in a local `data/` directory, which is intentionally excluded from version control.

---

## Technology Stack

- **Language:** C++  
- **Core Libraries:**
  - ITK (image I/O, registration, segmentation)
  - Eigen (math / linear algebra)
- **Build System:** CMake  
- **Visualization (planned):**
  - VTK
  - Qt  

The pipeline operates entirely in **physical coordinate space** and preserves orientation metadata throughout.
