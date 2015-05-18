#if !defined _REFRACT_VISITABLE_BY_H_
#define _REFRACT_VISITABLE_BY_H_

#include <stdexcept>
#include "Typelist.h"

template <typename Tlist> struct VisitableBy;

template <typename H, typename T>
struct VisitableBy< typelist::typelist<H,T> > : public VisitableBy<T> {
    template<typename Visitor, typename Arg>
    void InvokeVisit(Visitor& v, Arg& a) const {
        if(H* pFound = dynamic_cast<H*>(&v)) {
            pFound->visit(a);
        } else {
            VisitableBy<T>::InvokeVisit(v,a);
        }
    }
};

template <typename H>
struct VisitableBy< typelist::typelist<H, typelist::null_type> > {
    template<typename Visitor, typename Arg>
    void InvokeVisit(Visitor& v, Arg& a) const {
        if(H* pFound = dynamic_cast<H*>(&v)) {
            pFound->visit(a);
        } else {
            // IDEA: Inject policy instead of default throw
            throw std::runtime_error("Unknown visitor type");
        }
    }
};

#endif // #if !defined _REFRACT_VISITABLE_BY_H_
