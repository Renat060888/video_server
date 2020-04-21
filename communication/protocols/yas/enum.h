#ifndef ENUM_H
#define ENUM_H

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

namespace ParceArgs
{
template< unsigned N > constexpr
unsigned countArg( const char( &s )[N], unsigned i = 0, unsigned c = 0 )
{
    return
            s[i] == '\0'
            ? i == 0
            ? 0
            : c + 1
            : s[i] == ','
            ? countArg( s, i+1, c+1 )
            : countArg( s, i+1, c );
}

constexpr unsigned countArg()
{
    return 0;
}
}

//---------------------------------------

namespace StringifyEnum
{
static std::string TrimEnumString(const std::string &s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && isspace(*it)) { it++; }
    std::string::const_reverse_iterator rit = s.rbegin();
    while (rit.base() != it && isspace(*rit)) { ++rit; }
    return std::string(it, rit.base());
}

static std::vector<std::string> SplitEnumArgs(const char* szArgs, int     nMax)
{
    std::vector<std::string> enums;
    std::stringstream ss(szArgs);
    std::string strSub;
    int nIdx = 0;
    while (ss.good() && (nIdx < nMax)) {
        getline(ss, strSub, ',');
        enums.push_back(StringifyEnum::TrimEnumString(strSub));
        ++nIdx;
    }
    return std::move(enums);
}
}

//-------------------------------------------

#define DEFAULT_UNDEFINE Undefine
#define DEFAULT_FIRST FIRST
#define DEFAULT_LAST LAST
#define DEFAULT_END END
#define DECLARE_ENUM(ename, ...) DECLARE_ENUM_WITH_DEFAULT( ename, DEFAULT_UNDEFINE, __VA_ARGS__ )
#define DECLARE_ENUM_WITH_DEFAULT(ename, undef, ...) \
    enum class ename { undef = 0, __VA_ARGS__, \
        DEFAULT_END, DEFAULT_FIRST = 1, DEFAULT_LAST = DEFAULT_END-1 }; \
    static const int ename##n = ParceArgs::countArg( #__VA_ARGS__ ); \
    static std::vector<std::string> ename##Strings = StringifyEnum::SplitEnumArgs(#__VA_ARGS__, ename##n); \
    inline static std::string toString##ename(ename e) { \
        if( e == ename::Undefine ) return #undef; \
        return ename##Strings.at((int)e - 1); \
    } \
    inline static ename stringTo##ename(const std::string& en) { \
        const auto it = std::find(ename##Strings.begin(), ename##Strings.end(), en); \
        if (it != ename##Strings.end()) \
            return (ename) (std::distance(ename##Strings.begin(), it) + 1); \
        return ename::Undefine;     \
    } \
    inline static std::string def##ename(){ return #undef; }

#endif // ENUM_H
