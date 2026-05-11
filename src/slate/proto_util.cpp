#include "proto_util.hpp"
#include <format>

namespace slate {

void insert_metadata(VarMap& map, const google::protobuf::Message& message)
{
    const auto desc = message.GetDescriptor();
    const auto refl = message.GetReflection();
    for (auto i = 0; i < desc->field_count(); ++i) {
        const auto field_desc = desc->field(i);
        const auto field_name = std::string(field_desc->name());
        const auto cpp_type = field_desc->cpp_type();
        const auto type     = field_desc->type();
        if (field_desc->is_repeated())
            continue; // Unimplemented for now.
        if (!refl->HasField(message, field_desc))
            continue;

        switch(cpp_type) {
        using T  = google::protobuf::FieldDescriptor::CppType;
        using T_ = google::protobuf::FieldDescriptor::Type;
        case(T::CPPTYPE_INT32):
            map[field_name] = (types::Int)refl->GetInt32(message, field_desc);
            break;
        case(T::CPPTYPE_INT64):
            map[field_name] = (types::Int)refl->GetInt64(message, field_desc);
            break;
        case(T::CPPTYPE_UINT32):
            map[field_name] = (types::UInt)refl->GetUInt32(message, field_desc);
            break;
        case(T::CPPTYPE_UINT64):
            map[field_name] = (types::UInt)refl->GetUInt64(message, field_desc);
            break;
        case(T::CPPTYPE_DOUBLE):
            map[field_name] = refl->GetDouble(message, field_desc);
            break;
        case(T::CPPTYPE_FLOAT):
            map[field_name] = refl->GetFloat(message, field_desc);
            break;
        case(T::CPPTYPE_BOOL):
            map[field_name] = refl->GetBool(message, field_desc);
            break;
        case(T::CPPTYPE_STRING):
            if (type == T_::TYPE_STRING) {
                map[field_name] = refl->GetString(message, field_desc);
            } else if (type == T_::TYPE_BYTES) { // For byte array, convert into a vector of bytes.
                const auto str = refl->GetString(message, field_desc);
                types::VecBytes vec(str.cbegin(), str.cend());
                map[field_name] = std::move(vec);
            }
            break;
        case(T::CPPTYPE_ENUM):
        {
            const auto enum_desc = field_desc->enum_type();
            const auto enum_num = refl->GetEnumValue(message, field_desc);
            const auto enum_val_desc = enum_desc->FindValueByNumber(enum_num); // nullptr if num is invalid
            std::string display_name = std::format("{} ({})", (enum_val_desc ? enum_val_desc->name() : "Unknown value"),  enum_num);
            map[field_name] = std::move(display_name);
            break;
        }
        case(T::CPPTYPE_MESSAGE):
        {
            const auto& sub_msg = refl->GetMessage(message, field_desc);
            //const auto sub_msg_desc = field_desc->message_type();
            auto& inner_map = map[field_name].emplace<VarMap>();
            insert_metadata(inner_map, sub_msg); // Insert recursively.
            break;
        }
        default:
            break;
        }
    }
}

} // namespace slate
