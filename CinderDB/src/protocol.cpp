#include "cinderdb/protocol.hpp"
#include <cctype>
namespace cinderdb {
bool valid_key(const std::string_view key) {
 if (key.empty() || key.size() > 64) return false;
 for (const char raw : key) { const auto ch = static_cast<unsigned char>(raw); if (!(std::isalnum(ch) != 0 || ch == '_' || ch == '-' || ch == '.')) return false; }
 return true;
}
ParseResult parse_command(const std::string_view line) {
 if(line.empty()) return {{},"empty request"};
 if(line.size()>max_request_line_size) return {{},"request too large"};
 const auto first=line.find(' '); const std::string_view verb=line.substr(0,first); const std::string_view rest=first==std::string_view::npos ? std::string_view{} : line.substr(first+1);
 if(verb=="PUT") { const auto split=rest.find(' '); if(split==std::string_view::npos) return {{},"PUT requires key and value"}; auto key=rest.substr(0,split); auto value=rest.substr(split+1); if(!valid_key(key)) return {{},"invalid key"}; if(value.empty()) return {{},"PUT requires a nonempty value"}; return {Command{CommandType::put,std::string{key},std::string{value}}, {}}; }
 if(verb=="GET" || verb=="DELETE") { if(rest.empty() || rest.find(' ')!=std::string_view::npos) return {{},std::string{verb}+" requires exactly one key"}; if(!valid_key(rest)) return {{},"invalid key"}; return {Command{verb=="GET"?CommandType::get:CommandType::erase,std::string{rest},{}},{}}; }
 return {{},"unknown command"};
}
}  // namespace cinderdb
