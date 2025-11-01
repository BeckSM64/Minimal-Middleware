#include "IMmwMessageSerializer.h"

#if defined(CEREAL_SERIALIZER)
    #include "CerealSerializer.h"
#elif defined(JSON_SERIALIZER)
    #include "JsonSerializer.h"
#else
    #error "Invalid serializer provided"
#endif

IMmwMessageSerializer* CreateSerializer() {
#if defined(CEREAL_SERIALIZER)
    return new CerealSerializer();
#elif defined(JSON_SERIALIZER)
    return new JsonSerializer();
#endif
}
