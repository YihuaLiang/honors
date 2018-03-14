#include "operator_utilities.h"

bool
adds(const Operator &op, const Fluent &f) {
  const std::vector<PrePost> &pp_vec = op.get_pre_post();
  for(int i = 0; i < pp_vec.size(); i++) {
    const PrePost &pp = pp_vec[i];
    if(pp.var == f.first) {
      if(pp.post == f.second) {
        return true;
      }
      return false;
    }
  }
  return false;
}

bool
prevailed_by(const Operator &op, const Fluent &f) {
  const std::vector<Prevail> &pv_vec = op.get_prevail();
  for(int i = 0; i < pv_vec.size(); i++) {
    const Prevail &pv = pv_vec[i];
    if(pv.var == f.first) {
      if(pv.prev == f.second) {
        return true;
      }
      return false;
    }
  }
  return false;
}

bool
deletes(const Operator &op, const Fluent &f) {
  const std::vector<PrePost> &pp_vec = op.get_pre_post();
  for(std::vector<PrePost>::const_iterator it = pp_vec.begin();
      it != pp_vec.end(); it++) {
    const PrePost &pp = *it;
    if(pp.var == f.first) {
      if (pp.pre == f.second || (pp.pre == -1 && pp.post != f.second)) {
        return true;
      }
      return false;
    }
  }
  return false;
}

bool
deletes_precondition(const Operator &op, const Fluent &f) {
  const std::vector<PrePost> &pp_vec = op.get_pre_post();
  for(int i = 0; i < pp_vec.size(); i++) {
    const PrePost &pp = pp_vec[i];
    if(pp.var == f.first) {
      if(pp.pre == f.second) {
        return true;
      }
      return false;
    }
  }
  return false;
}

bool
impossible_pre(const Operator &op, const Fluent &f) {
  for(int i = 0; i < op.get_prevail().size(); i++) {
    const Prevail &pv = op.get_prevail()[i];
    if(are_mutex(f, std::make_pair(pv.var, pv.prev))) {
      return true;
    }
  }
  for(int i = 0; i < op.get_pre_post().size(); i++) {
    const PrePost &pp = op.get_pre_post()[i];
    if((pp.pre != -1) && are_mutex(f, std::make_pair(pp.var, pp.pre))) {
      return true;
    }
  }
  return false;
}

bool
impossible_post(const Operator &op, const Fluent &f) {
  for(int i = 0; i < op.get_prevail().size(); i++) {
    const Prevail &pv = op.get_prevail()[i];
    if(are_mutex(f, std::make_pair(pv.var, pv.prev))) {
      return true;
    }
  }
  for(int i = 0; i < op.get_pre_post().size(); i++) {
    const PrePost &pp = op.get_pre_post()[i];
    if(are_mutex(f, std::make_pair(pp.var, pp.post))) {
      return true;
    }
  }
  return false;
}

bool
e_deletes(const Operator &op, const Fluent &f) {
  const std::vector<Prevail> &pv_vec = op.get_prevail();
  for(std::vector<Prevail>::const_iterator it = pv_vec.begin();
      it != pv_vec.end(); it++) {
    const Prevail &pv = *it;
    if(are_mutex(std::make_pair(pv.var, pv.prev), f)) {
      // mutex with some prevail --> edeletes
      return true;
    }
  }

  bool mutex_w_pre_val = false;
  const std::vector<PrePost> &pp_vec = op.get_pre_post();
  for(std::vector<PrePost>::const_iterator it = pp_vec.begin();
      it != pp_vec.end(); it++) {
    const PrePost &pp = *it;
    if(are_mutex(std::make_pair(pp.var, pp.post), f)) {
      // mutex with some effect --> edeletes
      return true;
    }
    if(pp.var == f.first && pp.post == f.second) {
      // added by op --> does not edelete
      return false;
    }
    if(pp.pre != -1) {
      // mutex w/ pre val --> edeletes unless adds
      if(are_mutex(std::make_pair(pp.var, pp.pre), f)) {
        mutex_w_pre_val = true;
      }
    }
  }
  return mutex_w_pre_val;
}
