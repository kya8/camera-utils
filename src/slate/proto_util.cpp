#include "proto_util.hpp"
#include <format>
#include <vector>

namespace slate {

void insert_metadata(VarMap& map, const google::protobuf::Message& message)
{
    struct StackEntry {
        VarMap* map;
        const google::protobuf::Message* message;
    };

    // Pre-order DFS traversal with a stack
    std::vector<StackEntry> stack;
    stack.push_back({ &map, &message });

    while (!stack.empty()) {
        const auto [current_map, current_message] = stack.back();
        stack.pop_back();

        const auto desc = current_message->GetDescriptor();
        const auto refl = current_message->GetReflection();

        for (auto i = 0; i < desc->field_count(); ++i) {
            const auto field_desc = desc->field(i);
            const auto field_name = std::string(field_desc->name());
            const auto cpp_type = field_desc->cpp_type();
            const auto type     = field_desc->type();
            if (field_desc->is_repeated())
                continue; // Unimplemented for now.
            if (!refl->HasField(*current_message, field_desc))
                continue;

            switch(cpp_type) {
            using T  = google::protobuf::FieldDescriptor::CppType;
            using T_ = google::protobuf::FieldDescriptor::Type;
            case(T::CPPTYPE_INT32):
                (*current_map)[field_name] = (types::Int)refl->GetInt32(*current_message, field_desc);
                break;
            case(T::CPPTYPE_INT64):
                (*current_map)[field_name] = (types::Int)refl->GetInt64(*current_message, field_desc);
                break;
            case(T::CPPTYPE_UINT32):
                (*current_map)[field_name] = (types::UInt)refl->GetUInt32(*current_message, field_desc);
                break;
            case(T::CPPTYPE_UINT64):
                (*current_map)[field_name] = (types::UInt)refl->GetUInt64(*current_message, field_desc);
                break;
            case(T::CPPTYPE_DOUBLE):
                (*current_map)[field_name] = refl->GetDouble(*current_message, field_desc);
                break;
            case(T::CPPTYPE_FLOAT):
                (*current_map)[field_name] = refl->GetFloat(*current_message, field_desc);
                break;
            case(T::CPPTYPE_BOOL):
                (*current_map)[field_name] = refl->GetBool(*current_message, field_desc);
                break;
            case(T::CPPTYPE_STRING):
                if (type == T_::TYPE_STRING) {
                    (*current_map)[field_name] = refl->GetString(*current_message, field_desc);
                } else if (type == T_::TYPE_BYTES) { // For byte array, convert into a vector of bytes.
                    const auto str = refl->GetString(*current_message, field_desc);
                    types::VecBytes vec(str.cbegin(), str.cend());
                    (*current_map)[field_name] = std::move(vec);
                }
                break;
            case(T::CPPTYPE_ENUM):
            {
                const auto enum_desc = field_desc->enum_type();
                const auto enum_num = refl->GetEnumValue(*current_message, field_desc);
                const auto enum_val_desc = enum_desc->FindValueByNumber(enum_num); // nullptr if num is invalid
                std::string display_name = std::format("{} ({})", (enum_val_desc ? enum_val_desc->name() : "Unknown value"),  enum_num);
                (*current_map)[field_name] = std::move(display_name);
                break;
            }
            case(T::CPPTYPE_MESSAGE):
            {
                const auto& sub_msg = refl->GetMessage(*current_message, field_desc);
                //const auto sub_msg_desc = field_desc->message_type();
                auto& inner_map = (*current_map)[field_name].emplace<VarMap>();
                stack.push_back({ &inner_map, &sub_msg });
                break;
            }
            default:
                break;
            }
        }
    }
}

} // namespace slate
