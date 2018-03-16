
#ifndef NOGOODS_H
#define NOGOODS_H

#include <vector>
#include <cstdlib>

class State;

typedef int NoGoodID;
typedef std::vector<std::pair<int, int> > NoGood;

class NoGoods {
public:
    NoGoods(bool /*store*/) {}
    virtual ~NoGoods() {}
    virtual bool store(const NoGood &nogood) = 0;
    virtual NoGoodID match(const State &state) const = 0;
    virtual bool empty() const = 0;
    virtual size_t size() const = 0;
    virtual size_t memory() const = 0;
    virtual const NoGood &operator[](NoGoodID i) const = 0;
    static const NoGoodID NONOGOOD = -1;
};

class NoGoodsSingleVal : public NoGoods {
protected:
    bool _store_nogoods;
    std::vector<std::vector<std::pair<int, int> > > _nogoods;
    std::vector<int> _nogood_sizes;
    std::vector<std::vector<std::vector<int> > > _fact_to_nogood;
public:
    NoGoodsSingleVal(bool store);
    virtual bool store(const NoGood &nogood);
    virtual int match(const State &state) const;
    virtual bool empty() const;
    virtual size_t size() const;
    virtual size_t memory() const;
    virtual const NoGood &operator[](NoGoodID i) const;
};

class NoGoodsMultiVal : public NoGoodsSingleVal {
public:
    NoGoodsMultiVal(bool store);
    virtual bool store(const NoGood &nogood);
};

#endif

