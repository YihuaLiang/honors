#ifndef PLUGIN_H
#define PLUGIN_H

#include <vector>
#include <string>
#include <map>
#include <iostream>

#include "option_parser.h"

template <class T>
class Plugin {
    Plugin(const Plugin<T> &copy);
public:
    Plugin(const std::string &key, typename Registry<T *>::Factory factory) {
        Registry<T *>::
        instance()->register_object(key, factory);
    }
    ~Plugin() {}
};

#endif
