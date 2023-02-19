#include "others.pb.h"

union MessageProto{
    chat_proto::LoginRequest LRst;
    chat_proto::LoginResponse LRsp;
    chat_proto::RegisterRequest RRst;
    chat_proto::RegisterResponse RRsp;
    chat_proto::OneChatRequest OCRst;
    chat_proto::AddFriendRequest AFRst;
    chat_proto::AddFriendResponse AFRsp;
    chat_proto::CreateGroupRequest CGRst;
    chat_proto::CreateGroupResponse CGRsp;
    chat_proto::AddGroupRequest AGRst;
    chat_proto::AddGroupResponse AgRsp;
    chat_proto::GroupChatRequest GCRst;
};