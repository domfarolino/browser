#ifndef MAGE_CORE_HANDLES_H_
#define MAGE_CORE_HANDLES_H_

namespace mage {

// Typedefing this for explicitness. Each |MageHandle| references an underlying
// |Endpoint|, whose peer's address may be local or remote.
typedef uint32_t MageHandle;

}; // namspace mage

#endif // MAGE_CORE_HANDLES_H_
