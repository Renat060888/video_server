#ifndef VISITOR_METADATA_H
#define VISITOR_METADATA_H

#include "common/common_types.h"

#include "database_manager.h"
#include "objrepr_repository.h"

class MetadataWriteVisitor : public common_types::IMetadataVisitor {
    friend class StorageEngineFacade;
public:
    virtual ~MetadataWriteVisitor(){}

    virtual void visit( const common_types::SHlsMetadata * _metadata ) override;
    virtual void visit( const common_types::SBookmark * _metadata ) override;
    virtual void visit( const common_types::SOnceMoreMetadata * _metadata ) override;

private:
    ObjreprRepository * m_objrepr;
    DatabaseManager * m_database;

};

class MetadataDeleteVisitor : public common_types::IMetadataVisitor {
    friend class StorageEngineFacade;
public:
    virtual ~MetadataDeleteVisitor(){}

    virtual void visit( const common_types::SHlsMetadata * _metadata ) override;
    virtual void visit( const common_types::SBookmark * _metadata ) override;
    virtual void visit( const common_types::SOnceMoreMetadata * _metadata ) override;

private:
    ObjreprRepository * m_objrepr;
    DatabaseManager * m_database;

};

#endif // VISITOR_METADATA_H
