#ifndef UNIFIED_COMMAND_CONVERTOR_VS_H
#define UNIFIED_COMMAND_CONVERTOR_VS_H

#include <microservice_common/communication/unified_command_convertor.h>

class UnifiedCommandConvertorVS : public UnifiedCommandConvertor
{
public:
    static UnifiedCommandConvertorVS & singleton(){
        static UnifiedCommandConvertorVS instance;
        return instance;
    }


    virtual std::string getCommandFromProgramArgs( const std::map<common_types::TCommandLineArgKey, common_types::TCommandLineArgVal> & _args ) override;
    virtual std::string getCommandFromConfigFile( const std::string & _command ) override;
    virtual std::string getCommandFromHTTPRequest( const std::string & _httpMethod,
                                                    const std::string & _uri,
                                                    const std::string & _queryString,
                                                    const std::string & _body ) override;

private:
    UnifiedCommandConvertorVS();




};
#define UNIFIED_COMMAND_CONVERTOR UnifiedCommandConvertorVS::singleton()

#endif // UNIFIED_COMMAND_CONVERTOR_VS_H
