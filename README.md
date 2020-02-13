# MonSTR: de novo tandem repeat mutations discovery.

#### Authors: * Ileena Mitra <ilmitra@ucsd.edu> * Melissa Gymrek <mgymrek@ucsd.edu> <br> License: GNU v2


Toolkit for calling and analyzing de novo tandem repeat mutations.
This repository is under construction but is planned for official release soon.

The core code for calling de novos is modified from that originally written by Thomas Willems in the [HipSTR](https://github.com/tfwillems/HipSTR) repository.



## Docker
The Dockerfile in this container sets up a Docker with GangSTR and dependencies installed. You can pull this image from https://hub.docker.com/r/ileena/cookiemonstr.

## Install

The typical installation time should be less than 10 minutes. Run the following command to install:
```
git clone https://github.com/gymreklab/STRDenovoTools  
cd STRDenovoTools  
./reconf  
./configure  
make  
make install  
cd ..  
```

## Software Dependencies

## Usage
```
CookieMonSTR --help
```

Demo example:
```
CookieMonSTR \
    --strvcf test.vcf.gz \
    --fam family.ped \
    --max-num-alleles 100 \
    --round-alleles --include-invariant \
    --output-all-loci \
    --gangstr \
    --require-all-children \
    --naive \
    --naive-expansions-frr 3,8 \
    --min-num-encl-child 3 --max-perc-encl-parent 0.05 --min-encl-match 0.9 --min-total-encl 10 \
    --out test_results.txt
```

The file used for --fam should be in pedigree file format (see more information here: https://gatk.broadinstitute.org/hc/en-us/articles/360035531972-PED-Pedigree-format).

## Cite 
If you use this method for your research, please cite:
Mitra, et al. 2020 
