/*
Copyright (C) 2017 Melissa Gymrek <mgymrek@ucsd.edu>

This file is part of STRDenovoTools.

STRDenovoTools is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

STRDenovoTools is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with STRDenovoTools.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <set>

#include <sys/stat.h>

#include "vcf_reader.h"

namespace VCF {

  const std::vector<std::string>& Variant::get_samples() const {
    return vcf_reader_->get_samples();
  }

  int Variant::num_alleles_by_length() const {
    std::set<int> lengths;
    for (auto iter=alleles_.begin(); iter!=alleles_.end(); iter++) {
      lengths.insert((int)iter->size());
    }
    return (int)lengths.size();
  }

  float Variant::heterozygosity() const {
    std::vector<double> allele_freqs(num_alleles(), 1.0); // Use a one sample pseudocount
    double total_count = num_alleles();
    // Get counts
    const std::vector<std::string> samples = get_samples();
    int gt_a, gt_b;
    for (auto iter = samples.begin(); iter != samples.end(); iter++) {
      if (sample_call_missing(*iter)) {
	continue;
      }
      get_genotype(*iter, gt_a, gt_b);
      allele_freqs[gt_a]++;
      allele_freqs[gt_b]++;
      total_count += 2;
    }
    // Compute heterozygosity
    double x = 0.0;
    for (int i = 0; i < allele_freqs.size(); i++) {
      double p = allele_freqs[i]/total_count;
      x += p*p;
    }
    return 1-x; 
  }

  float Variant::heterozygosity_by_length() const {
    std::vector<double> allele_freqs(num_alleles_by_length(), 1.0);
    double total_count = num_alleles_by_length();
    const std::vector<std::string> samples = get_samples();
    int gt_a, gt_b;
    for (auto iter = samples.begin(); iter != samples.end(); iter++) {
      if (sample_call_missing(*iter)) {
	continue;
      }
      get_genotype(*iter, gt_a, gt_b);
      allele_freqs[GetLengthIndexFromGT(gt_a)]++;
      allele_freqs[GetLengthIndexFromGT(gt_b)]++;
      total_count += 2;
    }
    // Compute heterozygosity
    double x = 0.0;
    for (int i = 0; i < allele_freqs.size(); i++) {
      double p = allele_freqs[i]/total_count;
      x += p*p;
    }
    return 1-x; 
  }

  void Variant::build_alleles_by_length() {
    // Build map of GT->allele length. Assume alleles ordered by length
    // Except reference allele, which is always first
    const std::vector<std::string> alleles = get_alleles();
    int ref_allele_size = (int) alleles.front().size();
    length_allele_sizes_.resize(num_alleles_by_length(), 0);
    length_allele_sizes_[0] = 0; //ref_allele_size;
    gt_to_length_index_[0] = 0;
    int allele_index = 1;
    int prev_index = 0;
    int allele_size = (int)alleles[1].size();
    for (int i = 1; i < alleles.size(); i++) {
      int len = (int)alleles[i].size();
      assert(len >= allele_size);
      if (len > allele_size) {
	if (len == ref_allele_size) { // If this alleles is ref length
	  allele_size = len;
	  prev_index = allele_index;
	  allele_index = 0;
	} else if (allele_size == ref_allele_size) { // If previous allele was ref length
	  allele_size = len;
	  allele_index = prev_index + 1;
	} else {
	  allele_size = len;
	  allele_index++;
      }
    }
      gt_to_length_index_[i] = allele_index;
      length_allele_sizes_[allele_index] = allele_size - ref_allele_size;
    }
  }

  int Variant::GetLengthIndexFromGT(const int& gt_index) const {
    return gt_to_length_index_.at(gt_index);
  }

  int Variant::GetSizeFromLengthAllele(const int& length_gt_index) const {
    return length_allele_sizes_[length_gt_index];
  }

  void Variant::get_genotype(const std::string& sample, int& gt_a, int& gt_b) const {
    int sample_index = vcf_reader_->get_sample_index(sample);
    if (sample_index == -1)
      gt_a = gt_b = -1;
    else {
      gt_a = gt_1_[sample_index];
      gt_b = gt_2_[sample_index];
    }
  }

  bool Variant::sample_call_missing(const std::string& sample) const {
    int sample_index = vcf_reader_->get_sample_index(sample);
    return (sample_index == -1 ? true : missing_[sample_index]);
  }

  void Variant::extract_alleles(){
    for (int i = 0; i < vcf_record_->n_allele; i++)
      alleles_.push_back(vcf_record_->d.allele[i]);
  }

  void Variant::extract_genotypes(){
    int   mem = 0;
    int* gts_ = NULL;
    std::string GT_KEY = "GT";
    int num_entries = bcf_get_format_int32(vcf_header_, vcf_record_, GT_KEY.c_str(), &gts_, &mem);
    if (num_entries <= 0)
      PrintMessageDieOnError("Failed to extract the genotypes from the VCF record", M_ERROR);
    if (num_entries != num_samples_ && num_entries != 2*num_samples_)
      PrintMessageDieOnError("Incorrect number of genotypes extracted from the VCF record", M_ERROR);

    if (num_entries == num_samples_){
      // When all sample genotypes are missing, the number of extracted GT's is the
      // same as the number of samples
      missing_ = std::vector<bool>(num_samples_, true);
      phased_  = std::vector<bool>(num_samples_, false);
      gt_1_    = std::vector<int>(num_samples_, -1);
      gt_2_    = std::vector<int>(num_samples_, -1);
    }
    else {
      missing_.reserve(num_samples_);
      phased_.reserve(num_samples_);
      gt_1_.reserve(num_samples_);
      gt_2_.reserve(num_samples_);
    
      int gt_index = 0;
      for (int i = 0; i < num_samples_; i++){
	if (bcf_gt_is_missing(gts_[gt_index]) || bcf_gt_is_missing(gts_[gt_index+1])){
	  missing_.push_back(true);
	  phased_.push_back(false);
	  gt_1_.push_back(-1);
	  gt_2_.push_back(-1);
	}
	else {
	  missing_.push_back(false);
	  phased_.push_back(bcf_gt_is_phased(gts_[gt_index+1]));
	  gt_1_.push_back(bcf_gt_allele(gts_[gt_index]));
	  gt_2_.push_back(bcf_gt_allele(gts_[gt_index+1]));
	}
	gt_index += 2;
      }
    }
    free(gts_);
  }

  void VCFReader::open(const std::string& filename){
    const char* cfilename = filename.c_str();
    
    if (bgzf_is_bgzf(cfilename) != 1)
      PrintMessageDieOnError("VCF file is not in a valid bgzipped file. Please ensure that bgzip was used to compress it", M_ERROR);
  
    char *fnidx = (char*) calloc(strlen(cfilename) + 5, 1);
    strcat(strcpy(fnidx, cfilename), ".tbi");
    struct stat stat_tbi, stat_vcf;
    stat(fnidx, &stat_tbi);
    stat(cfilename, &stat_vcf);
    if (stat_vcf.st_mtime > stat_tbi.st_mtime)
      PrintMessageDieOnError("The tabix index for the VCF file is older than the VCF itself. Please reindex the VCF with tabix", M_ERROR);
    free(fnidx);
  
    if ((vcf_input_ = hts_open(cfilename, "r")) == NULL)
      PrintMessageDieOnError("Failed to open the VCF file", M_ERROR);
    if ((tbx_input_ = tbx_index_load(cfilename)) == NULL)
      PrintMessageDieOnError("Failed to open the VCF file's tabix index", M_ERROR);
  
    int nseq;
    const char** seq = tbx_seqnames(tbx_input_, &nseq);
    for (int i = 0; i < nseq; i++)
      chroms_.push_back(seq[i]);
    free(seq);
  
    if (chroms_.size() == 0)
      PrintMessageDieOnError("VCF does not contain any chromosomes", M_ERROR);
  
    vcf_header_  = bcf_hdr_read(vcf_input_);
    tbx_iter_    = tbx_itr_querys(tbx_input_, chroms_.front().c_str());
    chrom_index_ = 0;
  
    for (int i = 0; i < bcf_hdr_nsamples(vcf_header_); i++){
      samples_.push_back(vcf_header_->samples[i]);
      sample_indices_[vcf_header_->samples[i]] = i;
    }
  }

  bool VCFReader::get_next_variant(Variant& variant){
    if ((tbx_iter_ != NULL) && tbx_itr_next(vcf_input_, tbx_input_, tbx_iter_, &vcf_line_) >= 0){
      if (vcf_parse(&vcf_line_, vcf_header_, vcf_record_) < 0)
	PrintMessageDieOnError("Failed to parse VCF record", M_ERROR);
      variant = Variant(vcf_header_, vcf_record_, this);
      return true;
    }
  
    if (jumped_)
      return false;
  
    while (chrom_index_+1 < chroms_.size()){
      chrom_index_++;
      tbx_itr_destroy(tbx_iter_);
      tbx_iter_ = tbx_itr_querys(tbx_input_, chroms_[chrom_index_].c_str());
    
      if ((tbx_iter_ != NULL) && tbx_itr_next(vcf_input_, tbx_input_, tbx_iter_, &vcf_line_) >= 0){
	if (vcf_parse(&vcf_line_, vcf_header_, vcf_record_) < 0)
	  PrintMessageDieOnError("Failed to parse VCF record", M_ERROR);
	variant = Variant(vcf_header_, vcf_record_, this);
	return true;
      }
    }
    return false;
  }

}
