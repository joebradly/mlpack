#ifndef HS_SPIKE_H
#define HS_SPIKE_H

#include "fastlib/fastlib.h"

class Spike {
 private:
  double time_;
  int label_;
  
 public:
  void Init(double time_in, int label_in) {
    time_ = time_in;
    label_ = label_in;
  }

  double time() {
    return time_;
  }
  
  int label() {
    return label_;
  }
};


class SpikeSeqPair {
 private:
  Vector x_;
  Vector y_;
  int n_spikes_;
  ArrayList<Spike> all_spikes;
  int tau_; // dependence horizon
  
 public:
  void Init(const char* x_filename,
	    const char* y_filename,
	    int tau_in) {
    tau_ = tau_in;
    LoadVector(x_filename, &x_);
    LoadVector(y_filename, &y_);
  }

  void LoadVector(const char* filename, Vector* vector) {
    ArrayList<double> linearized;
    linearized.Init();

    FILE* file = fopen(filename, "r");
    double num;
    while(fscanf(file, "%lf", &num) != EOF) {
      linearized.PushBackCopy(num);
    }
    fclose(file);

    linearized.Trim();
    
    int n_points = linearized.size();
    printf("n_points = %d\n", n_points);
    vector -> Own(linearized.ReleasePtr(), n_points);
  }

  void Merge() {
    n_spikes_ = x_.length() + y_.length();
    all_spikes.Init(n_spikes_);
    int i_x = 0;
    int i_y = 0;
    for(int i_all = 0; (i_x < x_.length()) || (i_y < y_.length()); i_all++) {
      if((i_x != x_.length()) &&
	 ((i_y == y_.length()) || (x_[i_x] <= y_[i_y]))) {
	all_spikes[i_all].Init(x_[i_x], 0);
	i_x++;
      }
      else {
	all_spikes[i_all].Init(y_[i_y], 1);
	i_y++;
      }
    }
  }

  void PrintAllSpikes() {
    for(int i = 0; i < all_spikes.size(); i++) {
      char label;
      if(all_spikes[i].label() == 0) {
	label = 'X';
      }
      else {
	label = 'Y';
      }
      printf("%f %c\n", all_spikes[i].time(), label);
    }
  }

  void XRef() {
    int cur_x = x_.length() - 1;
    int cur_y = y_.length() - 1;

    int n_gap = 0;
    printf("(%d,%d) %d\n", cur_x, cur_y, n_gap);
    while(x_[cur_x] <= y_[cur_y]) {
      n_gap++;
      cur_y--;
      printf("(%d,%d) %d\n", cur_x, cur_y, n_gap);
    }

    while(n_gap < tau_) {
      if(cur_y == -1 || cur_x <= 0) {
	FATAL("Dependence horizon is too large for the spike sequences. Decrease it.");
      }
      cur_x--;
      printf("(%d,%d) %d\n", cur_x, cur_y, n_gap);
     
      while(x_[cur_x] <= y_[cur_y]) {
	n_gap++;
	cur_y--;
	printf("(%d,%d) %d\n", cur_x, cur_y, n_gap);
	if(cur_y == -1) {
	  break;
	}
      } 
    }

    printf("cur_x by y constraints = %d\n", cur_x);


  }

  void ConstructPoints() {
    ConstructPointsByRefLabel(0);
    //ConstructPointsByRefLabel(1);
  }

  void ConstructPointsByRefLabel(int ref_label) {
    int min_ref_spike_num = FindMinSpikeReference(ref_label);
    printf("min_ref_spike_num = %d\n", min_ref_spike_num);

    int n_points = 0;
    for(int i = min_ref_spike_num; i < n_spikes_; i++) {
      if(all_spikes[i].label() == ref_label) {
	n_points++;
      }
    }
    
    Matrix primary_points;
    primary_points.Init(tau_, n_points);
    ConstructPointsByRefAndQueryLabel(min_ref_spike_num,
				      ref_label,
				      ref_label,
				      &primary_points);
    Matrix secondary_points;
    secondary_points.Init(tau_, n_points);
    ConstructPointsByRefAndQueryLabel(min_ref_spike_num,
				      ref_label,
				      1 - ref_label,
				      &secondary_points);

    primary_points.PrintDebug("primary points");
    secondary_points.PrintDebug("secondary points");
  }

  void ConstructPointsByRefAndQueryLabel(int min_ref_spike_num,
					 int ref_label,
					 int query_label,
					 Matrix* points) {
    int point_num = 0;
    for(int i = min_ref_spike_num; i < n_spikes_; i++) {
      if(all_spikes[i].label() == ref_label) {
	Vector point;
	points -> MakeColumnVector(point_num, &point);
	ConstructPoint(i, query_label, &point);
	point_num++;
      }
    }
  }
  
  void ConstructPoint(int ref_spike_num, int query_label, Vector* p_point) {
    printf("ref_spike_num = %d\n", ref_spike_num);
    Vector& point = *p_point;

    double ref_spike_time = all_spikes[ref_spike_num].time();

    int n_complete = 0;
    for(int i = ref_spike_num - 1; n_complete < tau_; i--) {
      if(all_spikes[i].label() == query_label) {
	point[n_complete] = ref_spike_time - all_spikes[i].time();
	n_complete++;
      }
    }
  }

  int FindMinSpikeReference(int ref_label) {
    int cur_spike = 0;

    int n_primary_spikes = 0; // spikes with the same label as ref_label
    int n_secondary_spikes = 0; // spikes with the label other than ref_label

    int n_spikes_minus_1 = n_spikes_ - 1;
    for(;
	((n_primary_spikes < tau_) || (n_secondary_spikes < tau_))
	  && (cur_spike < n_spikes_minus_1);
	cur_spike++) {
      if(all_spikes[cur_spike].label() == ref_label) {
	n_primary_spikes++;
      }
      else {
	n_secondary_spikes++;
      }
    }

    if((n_primary_spikes < tau_) || (n_secondary_spikes < tau_)) {
      FATAL("Dependence horizon is too large for the spike sequences. Decrease it.");
    }
    
    while((cur_spike < n_spikes_)
	  && (all_spikes[cur_spike].label() != ref_label)) {
      cur_spike++;
    }

    if(cur_spike == n_spikes_) {
      FATAL("Dependence horizon is too large for the spike sequences. Decrease it.");
    }

    return cur_spike;
  }

};


#endif /* HS_SPIKE_H */
