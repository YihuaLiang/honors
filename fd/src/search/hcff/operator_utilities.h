#ifndef OPERATOR_UTILITIES_H
#define OPERATOR_UTILITIES_H

#include "../globals.h"
#include "../operator.h"
#include "fluent_set_utilities.h"

bool adds(const Operator &op, const Fluent &f);
bool prevailed_by(const Operator &op, const Fluent &f);
bool deletes(const Operator &op, const Fluent &f);
bool deletes_precondition(const Operator &op, const Fluent &f);
bool impossible_pre(const Operator &op, const Fluent &f);
bool impossible_post(const Operator &op, const Fluent &f);
bool e_deletes(const Operator &op, const Fluent &f);

#endif
