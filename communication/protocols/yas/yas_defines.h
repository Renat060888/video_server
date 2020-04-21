#ifndef DEFINES_YAS_EVENTS_H
#define DEFINES_YAS_EVENTS_H

// #define YAS_EXTENDED

#ifdef YAS_EXTENDED
#define PROCESS( el, name ) ( #name, name )
#else
#define PROCESS( el, name ) ( #name, el.name )
#endif

#define ARG_0( el )
#define ARG_1( el, name ) PROCESS( el, name )
#define ARG_2( el, name, ... ) PROCESS( el, name ), ARG_1( el, __VA_ARGS__ )
#define ARG_3( el, name, ... ) PROCESS( el, name ), ARG_2( el, __VA_ARGS__ )
#define ARG_4( el, name, ... ) PROCESS( el, name ), ARG_3( el, __VA_ARGS__ )
#define ARG_5( el, name, ... ) PROCESS( el, name ), ARG_4( el, __VA_ARGS__ )
#define ARG_6( el, name, ... ) PROCESS( el, name ), ARG_5( el, __VA_ARGS__ )
#define ARG_7( el, name, ... ) PROCESS( el, name ), ARG_6( el, __VA_ARGS__ )
#define ARG_8( el, name, ... ) PROCESS( el, name ), ARG_7( el, __VA_ARGS__ )
#define ARG_9( el, name, ... ) PROCESS( el, name ), ARG_8( el, __VA_ARGS__ )
#define ARG_10( el, name, ... ) PROCESS( el, name ), ARG_9( el, __VA_ARGS__ )
#define ARG_11( el, name, ... ) PROCESS( el, name ), ARG_10( el, __VA_ARGS__ )
#define ARG_12( el, name, ... ) PROCESS( el, name ), ARG_11( el, __VA_ARGS__ )
#define ARG_13( el, name, ... ) PROCESS( el, name ), ARG_12( el, __VA_ARGS__ )
#define ARG_14( el, name, ... ) PROCESS( el, name ), ARG_13( el, __VA_ARGS__ )
#define ARG_15( el, name, ... ) PROCESS( el, name ), ARG_14( el, __VA_ARGS__ )
#define ARG_16( el, name, ... ) PROCESS( el, name ), ARG_15( el, __VA_ARGS__ )

#define GET_NTH_ARG( _1, _2, _3, _4, _5, _6, _7, _8, \
    _9, _10, _11, _12, _13, _14, _15, _16, N, ... ) N
#define STRING_FOR_ALL( elem, ... ) \
    GET_NTH_ARG( __VA_ARGS__, \
    ARG_16, ARG_15, ARG_14, ARG_13, ARG_12, ARG_11, ARG_10, ARG_9, \
    ARG_8, ARG_7, ARG_6, ARG_5, ARG_4, ARG_3, ARG_2, ARG_1, ARG_0 ) \
    ( elem, __VA_ARGS__ )

//------------------

#define YAS_DECLARE_STRUCT( type, ... ) \
template< typename YasArg > \
void serialize( YasArg& ar, type& elem ) \
{ \
    ar & YAS_OBJECT_NVP( \
            #type, \
                STRING_FOR_ALL( elem, __VA_ARGS__ ) \
            ); \
}

#define YAS_DECLARE_STRUCT_EXT( type, elem, ... ) \
static type elem; \
template< typename YasArg > \
void serialize( YasArg& ar, type& elem ) \
{ \
    ar & YAS_OBJECT_NVP( \
            #type, \
                STRING_FOR_ALL( elem, __VA_ARGS__ ) \
            ); \
}

//--------------------

#define YAS_FRIEND( type ) \
    template< typename YasArg > \
    friend void serialize( YasArg& ar, type& coor )

#endif // DEFINES_YAS_EVENTS_H
