#ifndef PHARE_CORE_UTILITIES_POINT_POINT_H
#define PHARE_CORE_UTILITIES_POINT_POINT_H

#include "utilities/meta/meta_utilities.h"

#include <array>
#include <cstddef>

namespace PHARE
{
template<typename T, typename Index, typename Attempt = void>
struct has_subscript_operator : std::false_type
{
};


template<typename T, typename Index>
struct has_subscript_operator<T, Index,
                              tryToInstanciate<decltype(std::declval<T>()[std::declval<Index>()])>>
    : std::true_type
{
};


template<typename T, typename Index = int>
using is_subscriptable = std::enable_if_t<has_subscript_operator<T, Index>::value, dummy::type>;


template<typename Type, std::size_t dim>
class Point
{
public:
    static constexpr std::size_t dimension = dim;
    using type                             = Type;

    template<typename... Indexes>
    constexpr explicit Point(Indexes... index)
        : r{{index...}}
    {
        static_assert(sizeof...(Indexes) == dimension,
                      "Error dimension does match number of arguments");
    }


    constexpr Point(std::array<Type, dim> coords)
        : r{std::move(coords)}
    {
    }

    template<typename Container, is_subscriptable<Container> = dummy::value>
    Point(Container c)
    {
        for (int i = 0; i < dim; ++i)
        {
            r[i] = c[i];
        }
    }

    constexpr Point() { r.fill(static_cast<Type>(0)); }

    type& operator[](std::size_t i) { return r[i]; }

    type const& operator[](std::size_t i) const { return r[i]; }

private:
    std::array<Type, dim> r;
};




} // namespace PHARE

#endif
