
#include "visitor_metadata.h"

using namespace std;
using namespace common_types;

// write
void MetadataWriteVisitor::visit( const SHlsMetadata * _metadata ){

    m_database->writeHlsMetadata( * _metadata );
//    m_objrepr->updateArchiveStatus( * _metadata );
}

void MetadataWriteVisitor::visit( const SBookmark * _metadata ){

    m_database->insertBookmark( * _metadata );
}

void MetadataWriteVisitor::visit( const SOnceMoreMetadata * _metadata ){

}


// delete
void MetadataDeleteVisitor::visit( const SHlsMetadata * _metadata ){

    m_database->removeHlsStream( * _metadata );
}

void MetadataDeleteVisitor::visit( const SBookmark * _metadata ){

    m_database->deleteBookmark( * _metadata );
}

void MetadataDeleteVisitor::visit( const SOnceMoreMetadata * _metadata ){

}
