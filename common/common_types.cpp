
#include "common_types.h"

using namespace std;
using namespace common_types;

// event
void SAnalyticEvent::accept( IEventVisitor * _visitor ) const {
    _visitor->visit( this );
}

void SAnalyzeStateEvent::accept( IEventVisitor * _visitor ) const {
    _visitor->visit( this );
}

void SArchiverStateEvent::accept( IEventVisitor * _visitor ) const {
    _visitor->visit( this );
}

void SOnceMoreEvent::accept( IEventVisitor * _visitor ) const {
    _visitor->visit( this );
}

// internal event
void SAnalyzedObjectEvent::accept( IEventInternalVisitor * _visitor ) const {
    _visitor->visit( this );
}

// metadata
void SHlsMetadata::accept( IMetadataVisitor * _visitor ) const {
    _visitor->visit( this );
}

void SBookmark::accept( IMetadataVisitor * _visitor ) const {
    _visitor->visit( this );
}
