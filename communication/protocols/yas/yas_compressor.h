#ifndef YAS_COMPRESSOR_H
#define YAS_COMPRESSOR_H

#include <iostream>

#include <yas/serialize.hpp>
#include <yas/std_types.hpp>

namespace yas_compressor
{

#define YAS_PROTOCOL_JSON yas::json
#define YAS_PROTOCOL_BIN yas::binary

#define VALUE_TO_BYTES( value, bytes, yas_protocol ) \
{ \
    constexpr std::size_t flags = yas::mem | yas_protocol; \
    yas::shared_buffer buf = yas::save< flags >( value ); \
    bytes = std::string( buf.data.get(), buf.size ); \
}

#define BYTES_TO_VALUE( bytes, value, yas_protocol ) \
{ \
    constexpr std::size_t flags = yas::mem | yas_protocol; \
    yas::shared_buffer buf( bytes.data(), bytes.size() ); \
    yas::load< flags >( buf, value ); \
}

enum class SerializeMode
{
    AUTO,
    JSON,
    BIN
};

class YasCompressor
{
public:
    static void setMode( SerializeMode mode )
    {
        getMode() = mode;
    }

    template< typename Result, typename Data >
    static Result decompressData( Data& bytes, SerializeMode mode = SerializeMode::AUTO  )
    {
        Result result;
        if( mode == SerializeMode::AUTO )
            mode = getMode();

        try
        {
            switch( mode )
            {
            case SerializeMode::JSON:
                BYTES_TO_VALUE( bytes, result, YAS_PROTOCOL_JSON );
                break;

            case SerializeMode::BIN:
                BYTES_TO_VALUE( bytes, result, YAS_PROTOCOL_BIN );
                break;

            case SerializeMode::AUTO:
                break;
            }
        }
        catch( ... )
        {
            std::cerr << "Can't decompress Data" << std::endl;
        }

        return result;
    }

    template< typename Source >
    static std::string compressData( Source& source, SerializeMode mode = SerializeMode::AUTO )
    {
        std::string res;
        if( mode == SerializeMode::AUTO )
            mode = getMode();

        try
        {
            switch( mode )
            {
            case SerializeMode::JSON:
                VALUE_TO_BYTES( source, res, YAS_PROTOCOL_JSON );
                break;

            case SerializeMode::BIN:
                VALUE_TO_BYTES( source, res, YAS_PROTOCOL_BIN );
                break;

            case SerializeMode::AUTO:
                break;
            }
        }
        catch( ... )
        {
            std::cerr << "Can't compress Data" << std::endl;
        }

        return res;
    }

private:
    static SerializeMode& getMode()
    {
        static SerializeMode mode = SerializeMode::JSON;
        return mode;
    }
};

}

#endif // YAS_COMPRESSOR_H
