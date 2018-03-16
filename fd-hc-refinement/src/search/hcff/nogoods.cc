
#include "nogoods.h"

#include "../state.h"
#include "../globals.h"

using namespace std;

/*******************************************************************************
NOGOODS
*******************************************************************************/

NoGoodsSingleVal::NoGoodsSingleVal(bool store)
    : NoGoods(store), _store_nogoods(store)
{
    _fact_to_nogood.resize(g_variable_domain.size());
    for (size_t var = 0; var < g_variable_domain.size(); var++) {
        _fact_to_nogood[var].resize(g_variable_domain[var]);
    }
}

const std::vector<std::pair<int, int> > &NoGoodsSingleVal::operator[](NoGoodID i) const
{
    return _nogoods[i];
}

bool NoGoodsSingleVal::store(const std::vector<std::pair<int, int> > &nogood)
{
    if (_store_nogoods) {
        _nogoods.push_back(nogood);
    }
    //bool exists = false;
    //vector<int> s;
    //s.resize(_nogood_sizes.size(), 0);
    //for (size_t i = 0; i < nogood.size(); i++) {
    //    const vector<int> &ind = _fact_to_nogood[nogood[i].first][nogood[i].second];
    //    for (int j : ind) {
    //        if (++s[j] == _nogood_sizes[j]) {
    //            exists = true;
    //            break;
    //        }
    //    }
    //}
    //s.clear();
    //if (exists) {
    //    return false;
    //}
    for (size_t i = 0; i < nogood.size(); i++) {
        _fact_to_nogood[nogood[i].first][nogood[i].second].push_back(
            _nogood_sizes.size());
    }
    _nogood_sizes.push_back(nogood.size());
    return true;
}

int NoGoodsSingleVal::match(const State &state) const
{
    vector<int> matches;
    matches.resize(_nogood_sizes.size(), 0);
    for (size_t var = 0; var < g_variable_domain.size(); var++) {
        const vector<int> &ind = _fact_to_nogood[var][state[var]];
        for (int i : ind) {
            matches[i]++;
            if (matches[i] == _nogood_sizes[i]) {
                matches.clear();
                return i;
            }
        }
    }
    matches.clear();
    return NoGoods::NONOGOOD;
}

bool NoGoodsSingleVal::empty() const
{
    return _nogood_sizes.size() > 0;
}

size_t NoGoodsSingleVal::size() const
{
    return _nogood_sizes.size();
}

size_t NoGoodsSingleVal::memory() const
{
    size_t res = _nogood_sizes.size();
    for (size_t var = 0; var < _fact_to_nogood.size(); var++) {
        for (size_t val = 0; val < _fact_to_nogood[var].size(); val++) {
            res += _fact_to_nogood[var][val].size();
        }
    }
    return res * sizeof(int); // approximated size in bytes -- data structure offset not counted
}

NoGoodsMultiVal::NoGoodsMultiVal(bool store)
    : NoGoodsSingleVal(store)
{
}

bool NoGoodsMultiVal::store(const std::vector<std::pair<int, int> > &nogood)
{
    if (_store_nogoods) {
        _nogoods.push_back(nogood);
    }
    //bool exists = false;
    //vector<int> s;
    //s.resize(_nogood_sizes.size(), 0);
    //for (size_t i = 0; i < nogood.size(); i++) {
    //    const vector<int> &ind = _fact_to_nogood[nogood[i].first][nogood[i].second];
    //    for (int j : ind) {
    //        if (++s[j] == _nogood_sizes[j]) {
    //            exists = true;
    //            break;
    //        }
    //    }
    //}
    //s.clear();
    //if (exists) {
    //    return false;
    //}
    size_t size = 0;
    int last_var = -1;
    for (size_t i = 0; i < nogood.size(); i++) {
        if (nogood[i].first != last_var) {
            size++;
            last_var = nogood[i].first;
        }
        _fact_to_nogood[nogood[i].first][nogood[i].second].push_back(
            _nogood_sizes.size());
    }
    _nogood_sizes.push_back(size);
    return true;
}

