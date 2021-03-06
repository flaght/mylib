// Copyright (c) 2015-2015 The baselib Authors. All rights reserved.
// Created on: 2015年11月30日 Author: paco

#include "jsonp_serializer.h"
#include "string_escape.h"
#include "basic/basictypes.h"
#include "basic/basic_util.h"
#include "basic/scoped_ptr.h"
#include "logic_comm.h"
#include <math.h>
namespace base_logic{

static const Token kInvalidToken(Token::INVALID_TOKEN,0,0);
static const int kStackLimit = 100;

inline int HexToInt(wchar_t c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  } else if ('A' <= c && c <= 'F') {
    return c - 'A' + 10;
  } else if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  }
  return 0;
}

static const char kpretty_print_line_ending[] = "\n";

JsonpValueSerializer::JsonpValueSerializer(bool pretty_print)
:pretty_print_(pretty_print)
,stack_depth_(0)
,json_string_()
,jsonp_call_("")
,is_init_jsonp_(false){

}

JsonpValueSerializer::JsonpValueSerializer(std::string* json,bool pretty_print)
:pretty_print_(pretty_print)
,stack_depth_(0)
,json_string_()
,jsonp_call_("")
,is_init_jsonp_(false){

}

JsonpValueSerializer::~JsonpValueSerializer(){

}

bool JsonpValueSerializer::NextStringMatch(const std::wstring& str) {
  for (size_t i = 0; i < str.length(); ++i) {
    if ('\0' == *jsonp_pos_)
      return false;
    if (*(jsonp_pos_ + i) != str[i])
      return false;
  }
  return true;
}

Value* JsonpValueSerializer::DecodeNumber(const Token& token) {
  const std::wstring num_wstring(token.begin,token.length);
  const std::string  num_string =  base::BasicUtil::StringConversions::WideToASCII(num_wstring);
  int32 num_int;
  //if(base::BasicUtil::StringUtil::StringToInt(num_string,&num_int))
    //return Value::CreateIntegerValue(num_int);

  int64 num_int64;
  if(base::BasicUtil::StringUtil::StringToInt64(num_string,&num_int64))
    return Value::CreateBigIntegerValue(num_int64);

  double num_double;

  if(base::BasicUtil::StringUtil::StringToDouble(num_string,&num_double)&&finite(num_double))
    return Value::CreateRealValue(num_double);

  return NULL;
}

Value* JsonpValueSerializer::DecodeString(const Token& token) {
  std::wstring decoded_str;
  decoded_str.reserve(token.length - 2);

  for (int i = 1; i < token.length - 1; ++i) {
    wchar_t c = *(token.begin + i);
    if ('\\' == c) {
      ++i;
      c = *(token.begin + i);
      switch (c) {
        case '"':
        case '/':
        case '\\':
          decoded_str.push_back(c);
          break;
        case 'b':
          decoded_str.push_back('\b');
          break;
        case 'f':
          decoded_str.push_back('\f');
          break;
        case 'n':
          decoded_str.push_back('\n');
          break;
        case 'r':
          decoded_str.push_back('\r');
          break;
        case 't':
          decoded_str.push_back('\t');
          break;
        case 'v':
          decoded_str.push_back('\v');
          break;

        case 'x':
          decoded_str.push_back((HexToInt(*(token.begin + i + 1)) << 4) +
                                HexToInt(*(token.begin + i + 2)));
          i += 2;
          break;
        case 'u':
          decoded_str.push_back((HexToInt(*(token.begin + i + 1)) << 12 ) +
                                (HexToInt(*(token.begin + i + 2)) << 8) +
                                (HexToInt(*(token.begin + i + 3)) << 4) +
                                HexToInt(*(token.begin + i + 4)));
          i += 4;
          break;

        default:
          // We should only have valid strings at this point.  If not,
          // ParseStringToken didn't do it's job.
          return NULL;
      }
    } else {
      // Not escaped
      decoded_str.push_back(c);
    }
  }
  return Value::CreateStringValue(decoded_str);
}

bool JsonpValueSerializer::EatComment() {
  if ('/' != *jsonp_pos_)
    return false;

  wchar_t next_char = *(jsonp_pos_ + 1);
  if ('/' == next_char) {
    // Line comment, read until \n or \r
    jsonp_pos_ += 2;
    while ('\0' != *jsonp_pos_) {
      switch (*jsonp_pos_) {
        case '\n':
        case '\r':
          ++jsonp_pos_;
          return true;
        default:
          ++jsonp_pos_;
      }
    }
  } else if ('*' == next_char) {
    // Block comment, read until */
    jsonp_pos_ += 2;
    while ('\0' != *jsonp_pos_) {
      if ('*' == *jsonp_pos_ && '/' == *(jsonp_pos_ + 1)) {
        jsonp_pos_ += 2;
        return true;
      }
      ++jsonp_pos_;
    }
  } else {
    return false;
  }
  return true;
}

void JsonpValueSerializer::FreeValue(base_logic::Value* value){
  //类别识别
  if(value->GetType()==base_logic::Value::TYPE_DICTIONARY){
    base_logic::DictionaryValue* dict = (base_logic::DictionaryValue*)(value);
    if(dict!=NULL){delete value;value=NULL;}
  }
}

Value* JsonpValueSerializer::Deserialize(std::string* str,
		  int* error_code, std::string* error_str) {
	json_string_ = str;
	std::wstring json_wide(base::BasicUtil::StringConversions::UTF8ToWide(*json_string_));
	start_pos_ = json_wide.c_str();
	if(!json_wide.empty() && start_pos_[0] == 0xFEFF)
		++start_pos_;
	jsonp_pos_ = start_pos_;
	allow_trailing_comma_ = true;
	stack_depth_ = 0;
	error_code_ = NO_ERROR;
	scoped_ptr<Value> root(BuildValue(true));
	if(root.get()){
		if(ParseToken().type == Token::END_OF_INPUT){ // 空值处理
			return root.release();
		}else{
			SetErrorCode(UNEXPECTED_DATA_AFTER_ROOT,jsonp_pos_);
		}
	}
	if(error_code == 0)
		SetErrorCode(SYNTAX_ERROR,jsonp_pos_);
	return root.release();
}

Value* JsonpValueSerializer::Deserialize(int* error_code,std::string* error_str){

	std::wstring json_wide(base::BasicUtil::StringConversions::UTF8ToWide(*json_string_));
	start_pos_ = json_wide.c_str();
	if(!json_wide.empty() && start_pos_[0] == 0xFEFF)
		++start_pos_;
	jsonp_pos_ = start_pos_;
	allow_trailing_comma_ = true;
	stack_depth_ = 0;
	error_code_ = NO_ERROR;
	scoped_ptr<Value> root(BuildValue(true));
	if(root.get()){
		if(ParseToken().type == Token::END_OF_INPUT){ // 空值处理
			return root.release();
		}else{
			SetErrorCode(UNEXPECTED_DATA_AFTER_ROOT,jsonp_pos_);
		}
	}
	if(error_code == 0)
		SetErrorCode(SYNTAX_ERROR,jsonp_pos_);
	return root.release();
}


Value* JsonpValueSerializer::BuildValue(bool is_root){
  ++stack_depth_;
  if(stack_depth_ > kStackLimit){
    //SetErrorCode(TOO_MUCH_NESTING,http_pos_);
    return NULL;
  }
  Token token = ParseToken();
    // The root token must be an array or an object.
    if (is_root && token.type != Token::OBJECT_BEGIN &&
        token.type != Token::ARRAY_BEGIN) {
      SetErrorCode(BAD_ROOT_ELEENT_TYPE, jsonp_pos_);
      return NULL;
    }

    scoped_ptr<Value> node;

    switch (token.type) {
      case Token::END_OF_INPUT:
      case Token::INVALID_TOKEN:
        return NULL;

      case Token::NULL_TOKEN:
        node.reset(Value::CreateNullValue());
        break;

      case Token::BOOL_TRUE:
        node.reset(Value::CreateBooleanValue(true));
        break;

      case Token::BOOL_FALSE:
        node.reset(Value::CreateBooleanValue(false));
        break;

      case Token::NUMBER:
        node.reset(DecodeNumber(token));
        if (!node.get())
          return NULL;
        break;

      case Token::STRING:
        node.reset(DecodeString(token));
        if (!node.get())
          return NULL;
        break;

      case Token::ARRAY_BEGIN:
        {
          jsonp_pos_ += token.length;
          token = ParseToken();

          node.reset(new ListValue());
          while (token.type != Token::ARRAY_END) {
            Value* array_node = BuildValue(false);
            if (!array_node)
              return NULL;
            static_cast<ListValue*>(node.get())->Append(array_node);

            // After a list value, we expect a comma or the end of the list.
            token = ParseToken();
            if (token.type == Token::LIST_SEPARATOR) {
              jsonp_pos_ += token.length;
              token = ParseToken();
              // Trailing commas are invalid according to the JSON RFC, but some
              // consumers need the parsing leniency, so handle accordingly.
              if (token.type == Token::ARRAY_END) {
                if (!allow_trailing_comma_) {
                  SetErrorCode(TRAILING_COMMA, jsonp_pos_);
                  return NULL;
                }
                // Trailing comma OK, stop parsing the Array.
                break;
              }
            } else if (token.type != Token::ARRAY_END) {
              // Unexpected value after list value.  Bail out.
              return NULL;
            }
          }
          if (token.type != Token::ARRAY_END) {
            return NULL;
          }
          break;
        }

      case Token::OBJECT_BEGIN:
        {
          jsonp_pos_ += token.length;
          token = ParseToken();

          node.reset(new DictionaryValue);
          while (token.type != Token::OBJECT_END) {
            if (token.type != Token::STRING) {
              SetErrorCode(UNQUOTED_DICTIONARY_KEY, jsonp_pos_);
              return NULL;
            }
            scoped_ptr<Value> dict_key_value(DecodeString(token));
            if (!dict_key_value.get())
              return NULL;

            // Convert the key into a wstring.
            std::wstring dict_key;
            bool success = dict_key_value->GetAsString(&dict_key);


            jsonp_pos_ += token.length;
            token = ParseToken();
            if (token.type != Token::OBJECT_PAIR_SEPARATOR)
              return NULL;

            jsonp_pos_ += token.length;
            token = ParseToken();
            Value* dict_value = BuildValue(false);
            if (!dict_value)
              return NULL;
            static_cast<DictionaryValue*>(node.get())->SetWithoutPathExpansion(
                dict_key, dict_value);

            // After a key/value pair, we expect a comma or the end of the
            // object.
            token = ParseToken();
            if (token.type == Token::LIST_SEPARATOR) {
              jsonp_pos_ += token.length;
              token = ParseToken();
              // Trailing commas are invalid according to the JSON RFC, but some
              // consumers need the parsing leniency, so handle accordingly.
              if (token.type == Token::OBJECT_END) {
                if (!allow_trailing_comma_) {
                  SetErrorCode(TRAILING_COMMA, jsonp_pos_);
                  return NULL;
                }
                // Trailing comma OK, stop parsing the Object.
                break;
              }
            } else if (token.type != Token::OBJECT_END) {
              // Unexpected value after last object value.  Bail out.
              return NULL;
            }
          }
          if (token.type != Token::OBJECT_END)
            return NULL;

          break;
        }

      default:
        // We got a token that's not a value.
        return NULL;
    }
    jsonp_pos_ += token.length;

    --stack_depth_;
    return node.release();
}

Token JsonpValueSerializer::ParseToken(){
    static const std::wstring kNullString(L"null");
    static const std::wstring kTrueString(L"true");
    static const std::wstring kFalseString(L"false");

    EatWhitesspaceAndComments();

    Token token(Token::INVALID_TOKEN, 0, 0);
    switch (*jsonp_pos_) {
      case '\0':
        token.type = Token::END_OF_INPUT;
        break;

      case 'n':
        if (NextStringMatch(kNullString))
          token = Token(Token::NULL_TOKEN, jsonp_pos_, 4);
        break;

      case 't':
        if (NextStringMatch(kTrueString))
          token = Token(Token::BOOL_TRUE, jsonp_pos_, 4);
        break;

      case 'f':
        if (NextStringMatch(kFalseString))
          token = Token(Token::BOOL_FALSE, jsonp_pos_, 5);
        break;

      case '[':
        token = Token(Token::ARRAY_BEGIN, jsonp_pos_, 1);
        break;

      case ']':
        token = Token(Token::ARRAY_END, jsonp_pos_, 1);
        break;

      case ',':
        token = Token(Token::LIST_SEPARATOR, jsonp_pos_, 1);
        break;

      case '{':
        token = Token(Token::OBJECT_BEGIN, jsonp_pos_, 1);
        break;

      case '}':
        token = Token(Token::OBJECT_END, jsonp_pos_, 1);
        break;

      case ':':
        token = Token(Token::OBJECT_PAIR_SEPARATOR, jsonp_pos_, 1);
        break;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '-':
        token = ParseNumberToken();
        break;

      case '"':
        token = ParseStringToken();
        break;
    }
    return token;
}

bool JsonpValueSerializer::ReadInt(Token& token, bool can_have_leading_zeros){
   wchar_t first = token.NextChar();
    int len = 0;

    // Read in more digits
    wchar_t c = first;
    while ('\0' != c && '0' <= c && c <= '9') {
      ++token.length;
      ++len;
      c = token.NextChar();
    }
    // We need at least 1 digit.
    if (len == 0)
      return false;

    if (!can_have_leading_zeros && len > 1 && '0' == first)
      return false;

    return true;
}

// A helper method for ParseStringToken.  It reads |digits| hex digits from the
// token. If the sequence if digits is not valid (contains other characters),
// the method returns false.
bool JsonpValueSerializer::ReadHexDigits(Token& token, int digits) {
  for (int i = 1; i <= digits; ++i) {
    wchar_t c = *(token.begin + token.length + i);
    if ('\0' == c)
      return false;
    if (!(('0' <= c && c <= '9') || ('a' <= c && c <= 'f') ||
          ('A' <= c && c <= 'F'))) {
      return false;
    }
  }

  token.length += digits;
  return true;
}

Token JsonpValueSerializer::ParseNumberToken() {
  // We just grab the number here.  We validate the size in DecodeNumber.
  // According   to RFC4627, a valid number is: [minus] int [frac] [exp]
  Token token(Token::NUMBER, jsonp_pos_, 0);
  wchar_t c = *jsonp_pos_;
  if ('-' == c) {
    ++token.length;
    c = token.NextChar();
  }

  if (!ReadInt(token, false))
    return kInvalidToken;

  // Optional fraction part
  c = token.NextChar();
  if ('.' == c) {
    ++token.length;
    if (!ReadInt(token, true))
      return kInvalidToken;
    c = token.NextChar();
  }

  // Optional exponent part
  if ('e' == c || 'E' == c) {
    ++token.length;
    c = token.NextChar();
    if ('-' == c || '+' == c) {
      ++token.length;
      c = token.NextChar();
    }
    if (!ReadInt(token, true))
      return kInvalidToken;
  }

  return token;
}

Token JsonpValueSerializer::ParseStringToken() {
  Token token(Token::STRING, jsonp_pos_, 1);
  wchar_t c = token.NextChar();
  while ('\0' != c) {
    if ('\\' == c) {
      ++token.length;
      c = token.NextChar();
      // Make sure the escaped char is valid.
      switch (c) {
        case 'x':
          if (!ReadHexDigits(token, 2)) {
            SetErrorCode(INVALID_ESCAPE, jsonp_pos_ + token.length);
            return kInvalidToken;
          }
          break;
        case 'u':
          if (!ReadHexDigits(token, 4)) {
            SetErrorCode(INVALID_ESCAPE, jsonp_pos_ + token.length);
            return kInvalidToken;
          }
          break;
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case 'v':
        case '"':
          break;
        default:
          SetErrorCode(INVALID_ESCAPE, jsonp_pos_ + token.length);
          return kInvalidToken;
      }
    } else if ('"' == c) {
      ++token.length;
      return token;
    }
    ++token.length;
    c = token.NextChar();
  }
  return kInvalidToken;
}

void JsonpValueSerializer::SetErrorCode(ParseError error,const wchar_t* error_pos){
  int line_number = 1;
  int colum_number =1;

  // Figure out the line and column the error occured at.
  for(const wchar_t* pos = start_pos_;pos!=error_pos;++pos){
    if(*pos == '\0')
      return;
    if(*pos == '\n'){
      ++line_number;
      colum_number = 1;
    }else{
      ++colum_number;
    }
  }

  error_line_ = line_number;
  error_col_ = colum_number;
  error_code_ = error;

}


void JsonpValueSerializer::EatWhitesspaceAndComments() {
  while ('\0' != *jsonp_pos_) {
    switch (*jsonp_pos_) {
      case ' ':
      case '\n':
      case '\r':
      case '\t':
        ++jsonp_pos_;
        break;
      case '/':
        // TODO(tc): This isn't in the RFC so it should be a parser flag.
        if (!EatComment())
          return;
        break;
      default:
        // Not a whitespace char, just exit.
        return;
    }
  }
}

bool JsonpValueSerializer::Serialize(const Value& root, std::string*  str) {
	json_string_ = str;
	BuildJsonpString(&root,0,false);
	if (jsonp_call_ != "") {
		*json_string_ = jsonp_call_ + std::string("(") + *json_string_ + std::string(")");
		is_init_jsonp_ = false;
		jsonp_call_ = "";
	}
	return true;
}

bool JsonpValueSerializer::Serialize(const Value& root){
  BuildJsonpString(&root,0,false);
  if (jsonp_call_ != "")
	*json_string_ = jsonp_call_ + std::string("(") + *json_string_ + std::string(")");
  return true;
}

void JsonpValueSerializer::BuildJsonpString(const Value* const node,int depth,bool escape){
  if(node==NULL){//node
    assert(0);
    return;
  }
  switch (node->GetType()){
    case Value::TYPE_NULL:
      json_string_->append("null");
      break;

    case Value::TYPE_BOOLEAN:
      {
       bool value;
       bool result = node->GetAsBoolean(&value);
       json_string_->append(value ? "true" : "false");
       break;
      }

	case Value::TYPE_CHAR_INTEGER:
	  {
		int8 value;
		bool result = node->GetAsCharInteger(&value);
		base::BasicUtil::StringUtil::StringAppendF(json_string_,"%1d",value);
		break;
	  }

	case Value::TYPE_SHORT_INTEGER:
	 {
		int16 value;
		bool result = node->GetAsShortInteger(&value);
		base::BasicUtil::StringUtil::StringAppendF(json_string_,"%2d",value);
		break;
	 }

    case Value::TYPE_INTEGER:
      {
        int32 value;
        bool result = node->GetAsInteger(&value);
        base::BasicUtil::StringUtil::StringAppendF(json_string_,"%d",value);
        break;
      }

    case Value::TYPE_BIG_INTEGER:
      {
        int64 value;
        bool  result = node->GetAsBigInteger(&value);
#if __LP64__
        base::BasicUtil::StringUtil::StringAppendF(json_string_,"%ld",value);
#else
        base::BasicUtil::StringUtil::StringAppendF(json_string_,"%lld",value);
#endif
        break;

      }
    case Value::TYPE_REAL:
      {
          double value;
          bool result = node->GetAsReal(&value);
          std::string real = base::BasicUtil::StringUtil::DoubleToString(value);
          if (real.find('.') == std::string::npos &&
                real.find('e') == std::string::npos &&
                real.find('E') == std::string::npos) {
              real.append(".0");
          }
            // The JSON spec requires that non-integer values in the range (-1,1)
            // have a zero before the decimal point - ".52" is not valid, "0.52" is.
          if (real[0] == '.') {
              real.insert(0, "0");
          } else if (real.length() > 1 && real[0] == '-' && real[1] == '.') {
              // "-.1" bad "-0.1" good
              real.insert(1, "0");
          }
          json_string_->append(real);
          break;
        }

    case Value::TYPE_STRING:
        {
         std::string value;
         bool result = node->GetAsString(&value);
         if(escape){//暂只支持UTF8

         }else{
           StringEscape::JsonDoubleQuote(value,true,json_string_);
         }
         break;

        }
    case Value::TYPE_LIST:
       {
         json_string_->append("[");
         if (pretty_print_)
           json_string_->append(" ");

         const ListValue* list = static_cast<const ListValue*>(node);
         for(size_t i = 0; i< list->GetSize(); ++i){
           if(i != 0){
             json_string_->append(",");
             if (pretty_print_)
               json_string_->append(" ");
           }
           Value* value = NULL;
           bool result = list->Get(i, &value);
           BuildJsonpString(value,depth,escape);
         }
         if (pretty_print_)
           json_string_->append(" ");
         json_string_->append("]");
         break;
       }

    case Value::TYPE_DICTIONARY:
       {
        json_string_->append("{");
        if (pretty_print_)
          json_string_->append(kpretty_print_line_ending);

        const DictionaryValue* dict =
            static_cast<const DictionaryValue*>(node);
        for(DictionaryValue::key_iterator key_itr = dict->begin_keys();
            key_itr != dict->end_keys();++key_itr){

         Value* value = NULL;
         bool result = dict->GetWithoutPathExpansion(*key_itr,&value);
 
         if(!is_init_jsonp_) {
           std::string keystr = base::BasicUtil::StringConversions::WideToASCII(*key_itr);
         
           if (keystr.compare("jsonpcallback") == 0) {
             value->GetAsString(&jsonp_call_);
             is_init_jsonp_ = true;
           }
         }

          if(key_itr != dict->begin_keys()){
            json_string_->append(",");
            if (pretty_print_)
              json_string_->append(kpretty_print_line_ending);
          }

//          Value* value = NULL;
//          bool result = dict->GetWithoutPathExpansion(*key_itr,&value);
       
          if(pretty_print_)
            IndentLine(depth + 1);
          AppendQuoteString(base::BasicUtil::StringConversions::WideToASCII(*key_itr));
          if(pretty_print_){
            json_string_->append(": ");
          }else{
            json_string_->append(":");
          }
          BuildJsonpString(value,depth+1,escape);
        }

        if(pretty_print_){
          json_string_->append(kpretty_print_line_ending);
          IndentLine(depth);
          json_string_->append("}");
        }else{
          json_string_->append("}");
        }
        break;

       }
  }
}

}
