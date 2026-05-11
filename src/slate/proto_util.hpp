#ifndef PROTO_UTIL_HPP_CBF9CC20_A76A_4277_9419_91F680660295
#define PROTO_UTIL_HPP_CBF9CC20_A76A_4277_9419_91F680660295

#include <google/protobuf/message.h>
#include <slate/varmap.hpp>

namespace slate {

// Insert protobuf messages into a VarMap.
// Maps protobuf data types to appropriate VarMap types.
// Nested protobuf messages are inserted recursively.
void insert_metadata(VarMap& map, const google::protobuf::Message& message);

} // namespace slate

#endif /* PROTO_UTIL_HPP_CBF9CC20_A76A_4277_9419_91F680660295 */
