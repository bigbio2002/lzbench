#ifndef _GLZAMODEL_H_
#define _GLZAMODEL_H_
enum { TOP = 1 << 24, BUF_SIZE = 0x40000 };
enum { UP_FREQ_SYM_TYPE = 1, FREQ_SYM_TYPE_BOT = 0x4000 };
enum { UP_FREQ_MTF_QUEUE_NUM2 = 4, UP_FREQ_MTF_QUEUE_NUM = 10, FREQ_MTF_QUEUE_NUM_BOT = 0x100 };
enum { UP_FREQ_MTF_QUEUE_POS = 3, FREQ_MTF_QUEUE_POS_BOT = 0x2000 };
enum { UP_FREQ_MTFG_QUEUE_POS = 8, FREQ_MTFG_QUEUE_POS_BOT = 0x4000 };
enum { UP_FREQ_SID = 3, FREQ_SID_BOT = 0x1000 };
enum { UP_FREQ_INST = 8, FREQ_INST_BOT = 0x8000 };
enum { UP_FREQ_ERG = 1, FREQ_ERG_BOT = 0x20 };
enum { UP_FREQ_WORD_TAG = 1, FREQ_WORD_TAG_BOT = 0x80 };
enum { UP_FREQ_FIRST_CHAR = 8, FREQ_FIRST_CHAR_BOT = 0x2000 };
enum { NOT_CAP = 0, CAP = 1 };
enum { LEVEL0 = 0, LEVEL1 = 1, LEVEL0_CAP = 2, LEVEL1_CAP = 3 };

void InitSymbolFirstChar(uint8_t trailing_char, uint8_t leading_char);
void InitFreqFirstChar(uint8_t trailing_char, uint8_t leading_char);
void InitFirstCharBin(uint8_t trailing_char, uint8_t leading_char, uint8_t code_length, uint8_t cap_symbol_defined,
    uint8_t cap_lock_symbol_defined);
void InitFirstCharBinBinary(uint8_t trailing_char, uint8_t leading_char, uint8_t code_length);
void InitTrailingCharBin(uint8_t trailing_char, uint8_t leading_char, uint8_t code_length);
void InitTrailingCharBinary(uint32_t trailing_char, uint8_t * symbol_lengths);
void InitBaseSymbolCap(uint8_t BaseSymbol, uint8_t max_symbol, uint8_t new_symbol_code_length,
    uint8_t * cap_symbol_defined_ptr, uint8_t * cap_lock_symbol_defined_ptr, uint8_t * symbol_lengths);
void UpFreqMtfQueueNum(uint8_t Context, uint8_t mtf_queue_number);
void DoubleRange();
void DoubleRangeDown();
void DecreaseLow(uint32_t range_factor);
void IncreaseLow(uint32_t range_factor);
void MultiplyRange(uint32_t range_factor);
void SetOutBuffer(uint8_t * bufptr);
void WriteOutBuffer(uint8_t value);
void EncodeDictType(uint8_t Context);
void EncodeNewType(uint8_t Context);
void EncodeMtfgType(uint8_t Context);
void EncodeMtfType(uint8_t Context);
void EncodeMtfQueueNum(uint8_t Context, uint8_t mtf_queue_number);
void EncodeMtfQueueNumLastSymbol(uint8_t Context, uint8_t mtf_queue_number);
void EncodeMtfQueuePos(uint8_t Context, uint8_t mtf_queue_number, uint8_t * mtf_queue_size, uint8_t queue_position);
void EncodeMtfgQueuePos(uint8_t Context, uint8_t queue_position);
void EncodeSID(uint8_t Context, uint8_t SIDSymbol);
void EncodeExtraLength(uint8_t Symbol);
void EncodeINST(uint8_t Context, uint8_t SIDSymbol, uint8_t Symbol);
void EncodeERG(uint8_t Context, uint8_t Symbol);
void EncodeWordTag(uint8_t Symbol);
void EncodeShortDictionarySymbol(uint8_t Length, uint16_t BinNum, uint16_t DictionaryBins, uint16_t CodeBins);
void EncodeLongDictionarySymbol(uint32_t BinCode, uint16_t BinNum, uint16_t DictionaryBins, uint8_t CodeLength,
    uint16_t CodeBins);
void EncodeBaseSymbol(uint32_t BaseSymbol, uint8_t Bits, uint32_t NumBaseSymbols);
void EncodeFirstChar(uint8_t Symbol, uint8_t SymType, uint8_t LastChar);
void EncodeFirstCharBinary(uint8_t Symbol, uint8_t LastChar);
void WriteInCharNum(uint32_t value);
void WriteOutCharNum(uint32_t value);
uint32_t ReadOutCharNum();
void InitEncoder(uint8_t max_regular_code_length, uint8_t num_inst_codes, uint8_t cap_encoded, uint8_t UTF8_compliant,
    uint8_t use_mtf, uint8_t use_mtfg);
void FinishEncoder();
void DecodeSymTypeStart(uint8_t Context);
uint8_t DecodeSymTypeCheckDict(uint8_t Context);
void DecodeSymTypeFinishDict(uint8_t Context);
uint8_t DecodeSymTypeCheckNew(uint8_t Context);
void DecodeSymTypeFinishNew(uint8_t Context);
uint8_t DecodeSymTypeCheckMtfg(uint8_t Context);
void DecodeSymTypeFinishMtfg(uint8_t Context);
void DecodeSymTypeFinishMtf(uint8_t Context);
void DecodeMtfQueueNumStart(uint8_t Context);
uint8_t DecodeMtfQueueNumCheck0(uint8_t Context);
void DecodeMtfQueueNumFinish0(uint8_t Context);
uint8_t DecodeMtfQueueNumFinish(uint8_t Context);
void DecodeMtfQueuePosStart(uint8_t Context, uint8_t mtf_queue_number, uint8_t * mtf_queue_size);
uint8_t DecodeMtfQueuePosCheck0(uint8_t Context, uint8_t mtf_queue_number);
void DecodeMtfQueuePosFinish0(uint8_t Context, uint8_t mtf_queue_number);
uint8_t DecodeMtfQueuePosFinish(uint8_t Context, uint8_t mtf_queue_number);
void DecodeMtfgQueuePosStart(uint8_t Context);
uint8_t DecodeMtfgQueuePosCheck0(uint8_t Context);
uint8_t DecodeMtfgQueuePosFinish0(uint8_t Context);
uint8_t DecodeMtfgQueuePosFinish(uint8_t Context);
void DecodeSIDStart(uint8_t Context);
uint8_t DecodeSIDCheck0(uint8_t Context);
uint8_t DecodeSIDFinish0(uint8_t Context);
uint8_t DecodeSIDFinish(uint8_t Context);
uint8_t DecodeINSTCheck0(uint8_t Context, uint8_t SIDSymbol);
uint8_t DecodeExtraLength();
void DecodeINSTStart(uint8_t Context, uint8_t SIDSymbol);
void DecodeINSTFinish0(uint8_t Context, uint8_t SIDSymbol);
uint8_t DecodeINSTFinish(uint8_t Context, uint8_t SIDSymbol);
uint8_t DecodeERG(uint8_t Context);
uint8_t DecodeWordTag();
uint16_t DecodeDictionaryBin(uint8_t FirstChar, uint8_t * lookup_bits, uint8_t * CodeLengthPtr, uint16_t DictionaryBins,
    uint8_t bin_extra_bits);
uint32_t DecodeBinCode(uint8_t Bits);
uint32_t DecodeBaseSymbol(uint8_t Bits, uint32_t NumBaseSymbols);
uint8_t DecodeFirstChar(uint8_t SymType, uint8_t LastChar);
uint8_t DecodeFirstCharBinary(uint8_t LastChar);
void InitDecoder(uint8_t max_regular_code_length, uint8_t num_inst_codes, uint8_t cap_encoded, uint8_t UTF8_compliant,
    uint8_t use_mtf, uint8_t use_mtfg, uint8_t * inbuf);
#endif
