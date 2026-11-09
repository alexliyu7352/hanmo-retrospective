// 原GD resident_slot_codec的物理合同在实际原生消费者中复用；只接受既有format1/2。
#include "resident_slot_codec.hpp"
#include <nlohmann/json.hpp>
#include <picosha2.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

namespace hanmo::resident::storage {
namespace {
using Json=nlohmann::json;
// 按原StrictJSON语法前扫，重复解码键和深度错误不能被JSON库覆盖或改成损坏回退。
class LegacyJsonGuard {
public:
    explicit LegacyJsonGuard(const std::string& text):text_(text){}
    // 消费单个完整值；原来允许的数字前缀之后有剩余文本时仍明确拒绝。
    std::string check(){value(0);space();if(error_.empty()&&cursor_!=text_.size())error_="TrailingData";return error_;}
private:
    const std::string& text_;std::size_t cursor_=0;std::string error_;
    // 只消费JSON四种空白，不扩大Unicode分隔许可。
    void space(){while(cursor_<text_.size()&&(text_[cursor_]==' '||text_[cursor_]=='\t'||text_[cursor_]=='\r'||text_[cursor_]=='\n'))++cursor_;}
    // 标点只在当前位置匹配；失败不越过未知输入。
    bool take(char c){space();if(cursor_<text_.size()&&text_[cursor_]==c){++cursor_;return true;}return false;}
    // 叶错误先保留；容器按原StrictJSON标点优先级处理，TooDeep始终不可覆写。
    void bad(const char* code){if(error_.empty())error_=code;}
    // 字符串仍交锁定JSON库解码，重复键按解码值判定而非转义文本判定。
    std::string string(){
        space();if(cursor_==text_.size()||text_[cursor_]!='"'){bad("ExpectedString");return {};}
        const auto start=cursor_++;
        while(cursor_<text_.size()){
            const auto c=static_cast<unsigned char>(text_[cursor_++]);
            if(c=='"'){
                try{return Json::parse(text_.begin()+static_cast<std::ptrdiff_t>(start),text_.begin()+static_cast<std::ptrdiff_t>(cursor_)).get<std::string>();}
                catch(const Json::exception&){bad("InvalidString");return {};}
            }
            if(c<32){--cursor_;bad("InvalidString");return {};}
            if(c=='\\'&&cursor_<text_.size())++cursor_;
        }
        bad("UnterminatedString");return {};
    }
    // 数字继续原合法前缀规则；非有限值是资源/范围拒绝，不能许可回读旧银行。
    void number(){
        const auto start=cursor_;auto position=cursor_;if(position<text_.size()&&text_[position]=='-')++position;
        if(position==text_.size()){bad("InvalidNumber");return;}
        if(text_[position]=='0')++position;
        else if(text_[position]>='1'&&text_[position]<='9'){while(position<text_.size()&&text_[position]>='0'&&text_[position]<='9')++position;}
        else{bad("InvalidNumber");return;}
        if(position+1<text_.size()&&text_[position]=='.'&&text_[position+1]>='0'&&text_[position+1]<='9'){position+=2;while(position<text_.size()&&text_[position]>='0'&&text_[position]<='9')++position;}
        if(position<text_.size()&&(text_[position]=='e'||text_[position]=='E')){
            auto end=position+1;if(end<text_.size()&&(text_[end]=='+'||text_[end]=='-'))++end;
            const auto digits=end;while(end<text_.size()&&text_[end]>='0'&&text_[end]<='9')++end;if(end>digits)position=end;
        }
        cursor_=position;const auto token=text_.substr(start,cursor_-start);if(!std::isfinite(std::strtod(token.c_str(),nullptr)))bad("NonFiniteNumber");
    }
    // 原层数上限32，容器有严格逗号/冒号/结束符，不接受尾逗号。
    void value(std::size_t depth){
        if(!error_.empty())return;
        if(depth>32){bad("TooDeep");return;}space();if(cursor_==text_.size()){bad("UnexpectedEnd");return;}
        const auto c=text_[cursor_];
        if(c=='"'){string();return;}
        if(c=='{'){
            ++cursor_;std::set<std::string> keys;if(take('}'))return;
            while(error_.empty()){
                auto key=string();if(!error_.empty())return;if(!keys.insert(std::move(key)).second){bad("DuplicateKey");return;}
                if(!take(':')){bad("ExpectedColon");return;}value(depth+1);if(error_=="TooDeep")return;
                if(take('}'))return;
                if(!take(','))error_="ExpectedComma";
            }return;
        }
        if(c=='['){++cursor_;if(take(']'))return;while(error_.empty()){value(depth+1);if(error_=="TooDeep")return;if(take(']'))return;if(!take(','))error_="ExpectedComma";}return;}
        if(c=='t'||c=='f'||c=='n'){for(const auto* literal:{"true","false","null"}){const auto length=std::char_traits<char>::length(literal);if(text_.compare(cursor_,length,literal)==0){cursor_+=length;return;}}bad("InvalidLiteral");return;}
        number();
    }
};
// 原StrictJSON先经Godot数字归一化；1.0/1e0可变整数，超int32安全范围仍不具TYPE_INT。
std::optional<Integer> legacy_integer(const Json& value){
    if(!value.is_number())return {};
    const auto number=value.get<double>();
    if(!std::isfinite(number)||std::floor(number)!=number||std::abs(number)>2147483647.0)return {};
    return static_cast<Integer>(number);
}
// 原保存身份按Unicode码点限长；实际文件入口另受auto/manual闭合集约束。
bool valid_id(const std::string& id){if(id.empty())return false;std::size_t count=0;for(const auto c:id)if((static_cast<unsigned char>(c)&0xc0)!=0x80)++count;return count<=128;}
// 摘要只接受64位小写十六进制，不能引入第二种等价编码。
bool valid_digest(const std::string& digest){return digest.size()==64&&digest.find_first_not_of("0123456789abcdef")==std::string::npos;}
// 字节长度先算base64扩张，已知超限时不分配编码副本。
std::size_t encoded_size(std::size_t bytes){return ((bytes+2)/3)*4;}
// 采用原标准base64字符表，空白、URL变体和中间等号均不合法。
int base64_value(char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;}
// 规范编码保末组填充，与旧Marshalls.raw_to_base64字节一致。
std::string base64_encode(const Bytes& bytes){
    static constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;out.reserve(encoded_size(bytes.size()));
    for(std::size_t i=0;i<bytes.size();i+=3){const auto left=bytes.size()-i;const int a=bytes[i],b=left>1?bytes[i+1]:0,c=left>2?bytes[i+2]:0;out+=alphabet[a>>2];out+=alphabet[((a&3)<<4)|(b>>4)];out+=left>1?alphabet[((b&15)<<2)|(c>>6)]:'=';out+=left>2?alphabet[c&63]:'=';}return out;
}
// 直接拒非零unused pad bits，不依赖JSON库或宽松解码后形成另一种base64表示。
std::optional<Bytes> base64_decode(const std::string& text){
    if(text.empty()||text.size()%4)return {};
    const auto padding=text.size()>=2&&text.compare(text.size()-2,2,"==")==0?2u:text.back()=='='?1u:0u;
    for(std::size_t i=0;i<text.size()-padding;++i)if(base64_value(text[i])<0)return {};
    if(padding==2&&(base64_value(text[text.size()-3])&15))return {};
    if(padding==1&&(base64_value(text[text.size()-2])&3))return {};
    Bytes result;result.reserve(text.size()/4*3-padding);
    for(std::size_t i=0;i<text.size();i+=4){const auto a=base64_value(text[i]),b=base64_value(text[i+1]);const auto c=text[i+2]=='='?0:base64_value(text[i+2]),d=text[i+3]=='='?0:base64_value(text[i+3]);result.push_back(static_cast<std::uint8_t>((a<<2)|(b>>4)));if(text[i+2]!='=')result.push_back(static_cast<std::uint8_t>((b<<4)|(c>>2)));if(text[i+3]!='=')result.push_back(static_cast<std::uint8_t>((c<<6)|d));}return result;
}
// 失败白名单沿原物理codec；未知错误保持不可回退。
ResidentSlotDecoded reject(std::string code){
    static const std::set<std::string> physical={"InvalidSaveHeader","InvalidSaveMetadata","ChecksumMismatch","InvalidResidentBase64","InvalidJSON","TrailingData","ExpectedString","InvalidString","UnterminatedString","UnexpectedEnd","DuplicateKey","ExpectedColon","ExpectedComma","InvalidLiteral","InvalidNumber"};
    ResidentSlotDecoded result;result.physical_corruption=physical.count(code)!=0;result.code=std::move(code);return result;
}
// 编码失败沿现有Outcome返回，不提供可被调用者伪造的语义认证标志。
Outcome<Bytes> encode_failure(const std::string& code){return {false,{},{{code,"resident_slot"},ErrorKind::rule,{},{},{},{}}};}
}
// 同原PicoSHA2库只哈希实际解码或冻结的字节。
std::string resident_slot_sha256(const Bytes& bytes){return picosha2::hash256_hex_string(bytes.begin(),bytes.end());}
// 只产生原字典排序的七/十一字段JSON；payload来自原核心，目录元数据不在这里复制。
Outcome<Bytes> encode_resident_slot(const Bytes& bytes,Integer generation,const std::string& slot,const std::optional<Bytes>& continuation){
    try{
        if(generation<0||generation>2147483647||!valid_id(slot)||bytes.empty())return encode_failure("InvalidSaveMetadata");
        const bool with=continuation&&!continuation->empty();
        if(bytes.size()>resident_slot_byte_limit||encoded_size(bytes.size())>=resident_slot_byte_limit||(continuation&&continuation->size()>resident_slot_continuation_limit))return encode_failure("SaveSizeLimit");
        const auto hash=resident_slot_sha256(bytes);Json header={{"magic","HM-RESIDENT-SAVE"},{"format_version",with?2:1},{"save_id",slot},{"generation",generation},{"payload_length",bytes.size()},{"payload_sha256",hash},{"payload_base64",""}};
        if(with){header["continuation_length"]=continuation->size();header["continuation_sha256"]=resident_slot_sha256(*continuation);header["continuation_payload_sha256"]=hash;header["continuation_base64"]="";}
        const auto overhead=header.dump().size();const auto body=encoded_size(bytes.size())+(with?encoded_size(continuation->size()):0);
        if(overhead>resident_slot_byte_limit||body>resident_slot_byte_limit-overhead)return encode_failure("SaveSizeLimit");
        header["payload_base64"]=base64_encode(bytes);if(with)header["continuation_base64"]=base64_encode(*continuation);
        auto text=header.dump();return {true,Bytes(text.begin(),text.end()),{}};
    }catch(const std::bad_alloc&){return encode_failure("StorageResourceLimit");}catch(const Json::exception&){return encode_failure("InvalidSaveMetadata");}
}
// 全部物理字段及摘要通过才返回独立载荷，未知魔数/版本先于已知格式字段检查。
ResidentSlotDecoded decode_resident_slot(const Bytes& bytes){
    try{
        if(bytes.empty())return reject("InvalidSaveHeader");
        if(bytes.size()>resident_slot_byte_limit)return reject("SaveSizeLimit");
        const std::string text(bytes.begin(),bytes.end());const auto syntax=LegacyJsonGuard(text).check();if(!syntax.empty())return reject(syntax);const auto header=Json::parse(text);
        if(!header.is_object())return reject("InvalidSaveHeader");
        if(header.contains("magic")&&header["magic"].is_string()){
            if(header["magic"]!="HM-RESIDENT-SAVE")return reject("UnsupportedSaveFormat");
            if(header.contains("format_version")){const auto version=legacy_integer(header["format_version"]);if(version&&*version!=1&&*version!=2)return reject("UnsupportedSaveVersion");}
        }
        for(const auto* key:{"magic","save_id","payload_sha256","payload_base64"})if(!header.contains(key)||!header[key].is_string())return reject("InvalidSaveHeader");
        for(const auto* key:{"format_version","generation","payload_length"})if(!header.contains(key)||!legacy_integer(header[key]))return reject("InvalidSaveHeader");
        const auto version=*legacy_integer(header["format_version"]);const bool with=version==2;
        if(with){for(const auto* key:{"continuation_sha256","continuation_payload_sha256","continuation_base64"})if(!header.contains(key)||!header[key].is_string())return reject("InvalidSaveHeader");if(!header.contains("continuation_length")||!legacy_integer(header["continuation_length"]))return reject("InvalidSaveHeader");}
        if(header.size()!=(with?11u:7u))return reject("InvalidSaveHeader");
        ResidentSlotEnvelope value;value.format_version=version;value.generation=*legacy_integer(header["generation"]);value.save_id=header["save_id"].get<std::string>();value.payload_sha256=header["payload_sha256"].get<std::string>();const auto length=*legacy_integer(header["payload_length"]);
        if(value.generation<0||!valid_id(value.save_id)||length<1||length>static_cast<Integer>(resident_slot_byte_limit)||!valid_digest(value.payload_sha256))return reject("InvalidSaveMetadata");
        auto payload=base64_decode(header["payload_base64"].get<std::string>());if(!payload||payload->empty())return reject("InvalidResidentBase64");
        if(payload->size()!=static_cast<std::size_t>(length)||resident_slot_sha256(*payload)!=value.payload_sha256)return reject("ChecksumMismatch");
        value.payload=std::move(*payload);
        if(with){
            const auto continuation_length=*legacy_integer(header["continuation_length"]);value.continuation_sha256=header["continuation_sha256"].get<std::string>();const auto binding=header["continuation_payload_sha256"].get<std::string>();
            if(continuation_length>static_cast<Integer>(resident_slot_continuation_limit))return reject("SaveSizeLimit");
            if(continuation_length<1||!valid_digest(value.continuation_sha256)||!valid_digest(binding))return reject("InvalidSaveMetadata");
            if(binding!=value.payload_sha256)return reject("ChecksumMismatch");
            const auto encoded=header["continuation_base64"].get<std::string>();if(encoded.size()>encoded_size(resident_slot_continuation_limit))return reject("SaveSizeLimit");auto continuation=base64_decode(encoded);if(!continuation||continuation->empty())return reject("InvalidResidentBase64");
            if(continuation->size()>resident_slot_continuation_limit)return reject("SaveSizeLimit");
            if(continuation->size()!=static_cast<std::size_t>(continuation_length)||resident_slot_sha256(*continuation)!=value.continuation_sha256)return reject("ChecksumMismatch");
            value.continuation=std::move(*continuation);
        }
        ResidentSlotDecoded result;result.ok=true;result.envelope=std::move(value);return result;
    }catch(const std::bad_alloc&){return reject("StorageResourceLimit");}catch(const Json::exception&){return reject("InvalidJSON");}
}
// 选代只依据旧物理封套，不因core语义拒绝而悄悄回退另一代。
ResidentSlotSelected select_resident_slot(const SlotBankPair& banks,const std::string& slot){
    ResidentSlotSelected result;bool corrupt=false;const Bytes* selected=nullptr;
    for(const auto bank:{SlotBank::a,SlotBank::b}){
        const auto& image=bank==SlotBank::a?banks.a:banks.b;if(!image.present)continue;auto parsed=decode_resident_slot(image.bytes);
        if(!parsed.ok){if(!parsed.physical_corruption){result.ok=false;result.code=parsed.code;return result;}corrupt=true;continue;}
        if(parsed.envelope.save_id!=slot){result.ok=false;result.code="SlotIdentityMismatch";return result;}
        if(result.ok&&parsed.envelope.generation==result.envelope.generation&&image.bytes!=*selected){result.ok=false;result.code="GenerationConflict";return result;}
        if(!result.ok||parsed.envelope.generation>result.envelope.generation){result.ok=true;result.bank=bank;result.envelope=std::move(parsed.envelope);selected=&image.bytes;}
    }
    if(!result.ok){result.not_found=!corrupt;result.physical_corruption=corrupt;result.code=corrupt?"CorruptSlot":"NotFound";}else result.recovered=corrupt;return result;
}
} // 新槽admission、stream和世界语义核验不在旧物理codec中伪实现。
