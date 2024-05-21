#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stdio.h>
#include <stdint.h>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#include "tinyxml2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "exoquant.h"

typedef struct anim_pattern_file {
	uint32_t dataOfs;
	uint16_t sizeX;
	uint16_t sizeY;
	uint16_t centerX;
	uint16_t centerY;
} AnimPatternFile;

typedef struct anim_pattern {
    std::vector<uint8_t> data;
	uint16_t sizeX;
	uint16_t sizeY;
	uint16_t centerX;
	uint16_t centerY;
} AnimPattern;

typedef struct anim_frame {
	uint16_t pat;
	int16_t time;
	uint8_t shiftX;
	uint8_t shiftY;
	uint8_t flip;
} AnimFrame;

typedef struct anim_data_file {
	uint32_t patternOfs;
	uint32_t bankListOfs;
	uint32_t imgDataOfs;
	uint32_t palDataOfs;
	uint16_t patCnt;
	uint16_t bankCnt;
	uint16_t srcW;
	uint16_t srcH;
	uint16_t pixSize;
	uint8_t palCnt;
} AnimDataFile;

typedef struct anim_data {
    std::vector< AnimPattern> pattern;
    std::vector<std::vector<AnimFrame>> bank;
    std::vector<uint8_t> palData;
	uint16_t srcW;
	uint16_t srcH;
	uint16_t pixSize;
} AnimData;

void XMLCheck(tinyxml2::XMLError error)
{
    if (error != tinyxml2::XML_SUCCESS) {
        std::cout << "tinyxml2 error " << tinyxml2::XMLDocument::ErrorIDToName(error) << std::endl;
        exit(1);
    }
}

void SetSeek(FILE* file, size_t ofs)
{
    fseek(file, ofs, SEEK_SET);
}

uint8_t ReadU8(FILE* file)
{
    uint8_t temp;
    fread(&temp, 1, 1, file);
    return temp;
}

int8_t ReadS8(FILE* file)
{
    return ReadU8(file);
}

uint16_t ReadU16(FILE* file)
{
    uint8_t temp[2];
    fread(&temp, 1, 2, file);
    return (temp[0] << 8) | temp[1];
}

int16_t ReadS16(FILE* file)
{
    return ReadU16(file);
}

uint32_t ReadU32(FILE* file)
{
    uint8_t temp[4];
    fread(&temp, 1, 4, file);
    return (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
}

int32_t ReadS32(FILE* file)
{
    return ReadU32(file);
}

float ReadFloat(FILE* file)
{
    int32_t raw = ReadS32(file);
    return *(float*)(&raw);
}

void WriteU8(FILE* file, uint8_t value)
{
    fwrite(&value, 1, 1, file);
}

void WriteS8(FILE* file, int8_t value)
{
    fwrite(&value, 1, 1, file);
}

void WriteU16(FILE* file, uint16_t value)
{
    uint8_t temp[2];
    temp[0] = value >> 8;
    temp[1] = value & 0xFF;
    fwrite(temp, 2, 1, file);
}

void WriteS16(FILE* file, int16_t value)
{
    int8_t temp[2];
    temp[0] = value >> 8;
    temp[1] = value & 0xFF;
    fwrite(temp, 2, 1, file);
}

void WriteU32(FILE* file, uint32_t value)
{
    uint8_t temp[4];
    temp[0] = value >> 24;
    temp[1] = (value >> 16) & 0xFF;
    temp[2] = (value >> 8) & 0xFF;
    temp[3] = value & 0xFF;
    fwrite(temp, 4, 1, file);
}

void WriteS32(FILE* file, int32_t value)
{
    int8_t temp[4];
    temp[0] = value >> 24;
    temp[1] = (value >> 16) & 0xFF;
    temp[2] = (value >> 8) & 0xFF;
    temp[3] = value & 0xFF;
    fwrite(temp, 4, 1, file);
}

void WriteFloat(FILE* file, float value)
{
    WriteS32(file, *(int32_t*)&value);
}

void ReadAnimFile(FILE* file, AnimData* data)
{
    AnimDataFile animFile;
    animFile.patternOfs = ReadU32(file);
    animFile.bankListOfs = ReadU32(file);
    animFile.imgDataOfs = ReadU32(file);
    animFile.palDataOfs = ReadU32(file);
    animFile.patCnt = ReadU16(file);
    animFile.bankCnt = ReadU16(file);
    animFile.srcW = ReadU16(file);
    animFile.srcH = ReadU16(file);
    animFile.pixSize = ReadU16(file);
    animFile.palCnt = ReadU8(file);
    data->srcW = animFile.srcW;
    data->srcH = animFile.srcH;
    data->pixSize = animFile.pixSize;
    for (size_t i = 0; i < animFile.patCnt; i++) {
        AnimPattern pattern;
        AnimPatternFile patternFile;
        SetSeek(file, animFile.patternOfs + (i * sizeof(AnimPatternFile)));
        patternFile.dataOfs = ReadU32(file);
        pattern.sizeX = patternFile.sizeX = ReadU16(file);
        pattern.sizeY = patternFile.sizeY = ReadU16(file);
        pattern.centerX = patternFile.centerX = ReadU16(file);
        pattern.centerY = patternFile.centerY = ReadU16(file);
        uint32_t dataSize = ((patternFile.sizeX * patternFile.sizeY * (animFile.pixSize & 0x7FFF)) + 7) / 8;
        SetSeek(file, patternFile.dataOfs);
        pattern.data.resize(dataSize);
        fread(&pattern.data[0], 1, dataSize, file);
        data->pattern.push_back(pattern);
    }
    for (size_t i = 0; i < animFile.bankCnt; i++) {
        std::vector<AnimFrame> bank;
        uint32_t dataOfs;
        uint16_t timeNum;
        SetSeek(file, animFile.bankListOfs + (i * 4));
        dataOfs = ReadU32(file);
        SetSeek(file, dataOfs);
        timeNum = ReadU16(file);
        for (size_t j = 0; j < timeNum; j++) {
            AnimFrame frame;
            frame.pat = ReadU16(file);
            frame.time = ReadU16(file);
            frame.shiftX = ReadU8(file);
            frame.shiftY = ReadU8(file);
            frame.flip = ReadU8(file);
            if (frame.time != -1) {
                bank.push_back(frame);
            }
        }
        data->bank.push_back(bank);
    }
    if (animFile.pixSize == 4 || animFile.pixSize == 8) {
        size_t count = animFile.palCnt;
        if (animFile.pixSize == 8 && count == 0) {
            count = 256;
        }
        SetSeek(file, animFile.palDataOfs);
        data->palData.resize(count * 2);
        fread(&data->palData[0], 1, count * 2, file);
    }
}

const char *GetFlipName(uint8_t value)
{
    const char* flipName[] = {
                "none",
                "x",
                "y",
                "xy"
    };
    return flipName[value & 0x3];
}

uint8_t GetFlipId(std::string name)
{
    std::map<std::string, uint8_t> flipMap = {
        { "none", 0 },
        { "x", 1 },
        { "y", 2 },
        { "xy", 3 }
    };
    return flipMap[name];
}

const char* GetTexFmtName(uint16_t pixSize)
{
    std::map< uint16_t, const char *> fmtMap = {
        { 0x4, "ci4" },
        { 0x8, "ci8" },
        { 0x10, "rgba16" },
        { 0x20, "rgba32" },
        { 0x8004, "i4" },
        { 0x8008, "i8" }
    };

    return fmtMap[pixSize];
}

uint16_t GetTexFmtPixSize(std::string name)
{
    std::map<std::string, uint16_t> fmtMap = {
        { "i4", 0x8004 },
        { "i8", 0x8008 },
        { "ci4", 0x4 },
        { "ci8", 0x8 },
        { "rgba16", 0x10 },
        { "rgba32", 0x20 }
    };
    if (fmtMap.count(name) < 1) {
        return 0x10;
    }
    return fmtMap[name];
}

void DecodeValueRGBA16(uint8_t* src, uint8_t *dst)
{
    uint16_t value;
    value = (src[0] << 8) | src[1];
    dst[0] = ((value >> 11) & 0x1F);
    dst[1] = ((value >> 6) & 0x1F);
    dst[2] = ((value >> 1) & 0x1F);
    dst[0] = (dst[0] * 255) / 31;
    dst[1] = (dst[1] * 255) / 31;
    dst[2] = (dst[2] * 255) / 31;
    dst[3] = (value & 0x1) ? 0xFF : 0x0;
}

void EncodeValueRGBA16(uint8_t* src, uint8_t* dst)
{
    uint8_t r, g, b, a;
    uint16_t newValue = 0;
    r = src[0] >> 3;
    g = src[1] >> 3;
    b = src[2] >> 3;
    a = src[3] != 0;
    newValue = (r << 11) | (g << 6) | (b << 1) | a;
    dst[0] = newValue >> 8;
    dst[1] = newValue & 0xFF;
}

void DecodeImageCI4(uint8_t* data, uint8_t* palData, uint16_t w, uint16_t h, uint8_t *outData)
{
    for (size_t i = 0; i < h; i++) {
        for (size_t j = 0; j < w; j++) {
            size_t pixIdx = (i * w) + j;
            size_t srcOfs = (i * (w/2)) + (j / 2);
            uint8_t srcIdx = 0;
            if (j % 2 != 0) {
                srcIdx = data[srcOfs] & 0xF;
            } else {
                srcIdx = (data[srcOfs] & 0xF0) >> 4;
            }
            DecodeValueRGBA16(&palData[srcIdx * 2], &outData[pixIdx * 4]);
        }
    }
}

void DecodeImageCI8(uint8_t* data, uint8_t* palData, uint16_t w, uint16_t h, uint8_t* outData)
{
    for (size_t i = 0; i < h; i++) {
        for (size_t j = 0; j < w; j++) {
            size_t pixIdx = (i * w) + j;
            uint8_t srcIdx = data[pixIdx];
            DecodeValueRGBA16(&palData[srcIdx * 2], &outData[pixIdx * 4]);
        }
    }
}

void DecodeImageRGBA16(uint8_t* data, uint16_t w, uint16_t h, uint8_t* outData)
{
    for (size_t i = 0; i < h; i++) {
        for (size_t j = 0; j < w; j++) {
            size_t pixIdx = (i * w) + j;
            DecodeValueRGBA16(&data[pixIdx * 2], &outData[pixIdx * 4]);
        }
    }
}

void DecodeImageRGBA32(uint8_t* data, uint16_t w, uint16_t h, uint8_t* outData)
{
    for (size_t i = 0; i < h; i++) {
        for (size_t j = 0; j < w; j++) {
            size_t pixIdx = (i * w) + j;
            size_t dataOfs = pixIdx * 4;
            outData[dataOfs] = data[dataOfs];
            outData[dataOfs+1] = data[dataOfs+1];
            outData[dataOfs+2] = data[dataOfs+2];
            outData[dataOfs+3] = data[dataOfs+3];
        }
    }
}

void DecodeImageI4(uint8_t* data, uint16_t w, uint16_t h, uint8_t* outData)
{
    for (size_t i = 0; i < h; i++) {
        for (size_t j = 0; j < w; j++) {
            size_t pixIdx = (i * w) + j;
            size_t srcOfs = (i * (w / 2)) + (j / 2);
            uint8_t intensity = 0;
            if (j % 2 != 0) {
                intensity = data[srcOfs] & 0xF;
            }
            else {
                intensity = (data[srcOfs] & 0xF0) >> 4;
            }
            intensity = (intensity * 255) / 15;
            outData[(pixIdx * 4)] = intensity;
            outData[(pixIdx * 4) + 1] = intensity;
            outData[(pixIdx * 4) + 2] = intensity;
            outData[(pixIdx * 4) + 3] = intensity;
        }
    }
}

void DecodeImageI8(uint8_t* data, uint16_t w, uint16_t h, uint8_t* outData)
{
    for (size_t i = 0; i < h; i++) {
        for (size_t j = 0; j < w; j++) {
            size_t pixIdx = (i * w) + j;
            uint8_t intensity = data[pixIdx];
            outData[(pixIdx * 4)] = intensity;
            outData[(pixIdx * 4) + 1] = intensity;
            outData[(pixIdx * 4) + 2] = intensity;
            outData[(pixIdx * 4) + 3] = intensity;
        }
    }
}

void DecodeImage(uint8_t* data, uint8_t* palData, uint16_t w, uint16_t h, uint16_t pixSize, std::vector<uint8_t> &outData)
{
    outData.resize(w * h * 4);
    switch (pixSize) {
        case 4:
            DecodeImageCI4(data, palData, w, h, &outData[0]);
            break;

        case 8:
            DecodeImageCI8(data, palData, w, h, &outData[0]);
            break;

        case 16:
            DecodeImageRGBA16(data, w, h, &outData[0]);
            break;

        case 32:
            DecodeImageRGBA32(data, w, h, &outData[0]);
            break;

        case 0x8004:
            DecodeImageI4(data, w, h, &outData[0]);
            break;

        case 0x8008:
            DecodeImageI8(data, w, h, &outData[0]);
            break;

        default:
            std::cout << "Invalid pixel size " << pixSize << std::endl;
            exit(1);
    }
}
void WriteOutFiles(std::string out, AnimData *data)
{
    std::string file_noext = out.substr(0, out.find_last_of('.'));
    tinyxml2::XMLDocument document;
    tinyxml2::XMLElement* root = document.NewElement("animdata");
    document.InsertFirstChild(root);
    root->SetAttribute("texfmt", GetTexFmtName(data->pixSize));
    for (auto& bank : data->bank) {
        tinyxml2::XMLElement *bank_elem = document.NewElement("bank");
        for (auto& frame : bank) {
            
            tinyxml2::XMLElement *frame_elem = document.NewElement("frame");
            frame_elem->SetAttribute("pattern", frame.pat);
            frame_elem->SetAttribute("time", frame.time);
            frame_elem->SetAttribute("flip", GetFlipName(frame.flip));
            bank_elem->InsertEndChild(frame_elem);
        }
        root->InsertEndChild(bank_elem);
    }
    size_t index = 0;
    for (auto& pattern : data->pattern) {
        tinyxml2::XMLElement* pattern_elem = document.NewElement("pattern");
        std::string img_path = file_noext + "_" + std::to_string(index) + ".png";
        std::vector<uint8_t> outData;
        uint8_t* palData = NULL;
        pattern_elem->SetAttribute("center_x", pattern.centerX);
        pattern_elem->SetAttribute("center_y", pattern.centerY);
        pattern_elem->SetAttribute("path", img_path.c_str());
        if (data->palData.size() != 0) {
            palData = &data->palData[0];
        }
        DecodeImage(&pattern.data[0], palData, pattern.sizeX, pattern.sizeY, data->pixSize, outData);
        stbi_write_png(img_path.c_str(), pattern.sizeX, pattern.sizeY, 4, &outData[0], 4 * pattern.sizeX);
        root->InsertEndChild(pattern_elem);
        index++;
    }
    XMLCheck(document.SaveFile(out.c_str()));

}


void DumpAnimFile(std::string in, std::string out)
{
    FILE* file = fopen(in.c_str(), "rb");
    AnimData animData;
    ReadAnimFile(file, &animData);
    fclose(file);
    WriteOutFiles(out, &animData);
}

struct ImageData {
    unsigned int centerX = 0;
    unsigned int centerY = 0;
    int sizeX = 0;
    int sizeY = 0;
    std::vector<uint8_t> dataRaw;
    std::vector<uint8_t> dataQuant;
};

void EncodeImageDataCI4(ImageData& image, std::vector<uint8_t>& dst)
{
    int w = image.sizeX;
    int h = image.sizeY;
    int outPitch = w / 2;
    dst.resize(outPitch * h);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            uint8_t dataVal = image.dataQuant[(i * w) + j] & 0xF;
            if (j % 2 != 0) {
                dst[(i * outPitch) + (j / 2)] |= dataVal;
            } else {
                dst[(i * outPitch) + (j / 2)] = dataVal << 4;
            }
        }
    }
}

void EncodeImageDataCI8(ImageData& image, std::vector<uint8_t>& dst)
{
    int w = image.sizeX;
    int h = image.sizeY;
    dst.resize(w * h);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            uint8_t dataVal = image.dataQuant[(i * w) + j];
            dst[(i * w) + j] = dataVal;
        }
    }
}

void EncodeImageDataRGBA16(ImageData& image, std::vector<uint8_t>& dst)
{
    int w = image.sizeX;
    int h = image.sizeY;
    dst.resize(w * h * 2);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            EncodeValueRGBA16(&image.dataRaw[((i * w) + j) * 4], &dst[((i * w) + j) * 2]);
        }
    }
}

void EncodeImageDataRGBA32(ImageData& image, std::vector<uint8_t>& dst)
{
    int w = image.sizeX;
    int h = image.sizeY;
    dst.resize(w * h * 4);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            size_t pixOfs = ((i * w) + j) * 4;
            dst[pixOfs] = image.dataRaw[pixOfs];
            dst[pixOfs + 1] = image.dataRaw[pixOfs + 1];
            dst[pixOfs + 2] = image.dataRaw[pixOfs + 2];
            dst[pixOfs + 3] = image.dataRaw[pixOfs + 3];
        }
    }
}

void EncodeImageDataI4(ImageData& image, std::vector<uint8_t>& dst)
{
    int w = image.sizeX;
    int h = image.sizeY;
    int outPitch = w / 2;
    dst.resize(outPitch * h);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            size_t srcOfs = ((i * w) + j) * 4;
            float intensity = (0.299f * image.dataRaw[srcOfs]) + (0.587f * image.dataRaw[srcOfs + 1]) + (0.114f * image.dataRaw[srcOfs + 2]);
            uint8_t dataVal;
            intensity *= 15.0f / 255.0f;
            dataVal = (uint8_t)(intensity + 0.5f) & 0xF;
            if (j % 2 != 0) {
                dst[(i * outPitch) + (j / 2)] |= dataVal;
            } else {
                dst[(i * outPitch) + (j / 2)] = dataVal << 4;
            }
        }
    }
}

void EncodeImageDataI8(ImageData& image, std::vector<uint8_t>& dst)
{
    int w = image.sizeX;
    int h = image.sizeY;
    dst.resize(w * h);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            size_t srcOfs = ((i * w) + j) * 4;
            float intensity = (0.299f * image.dataRaw[srcOfs]) + (0.587f * image.dataRaw[srcOfs + 1]) + (0.114f * image.dataRaw[srcOfs + 2]);
            uint8_t dataVal;
            intensity += 0.5f; //Round intensity values
            dst[(i * w) + j] = intensity;
        }
    }
}


void EncodeImageData(uint16_t pixSize, ImageData& image, std::vector<uint8_t>& dst)
{
    switch (pixSize) {
        case 4:
            EncodeImageDataCI4(image, dst);
            break;

        case 8:
            EncodeImageDataCI8(image, dst);
            break;

        case 16:
            EncodeImageDataRGBA16(image, dst);
            break;

        case 32:
            EncodeImageDataRGBA32(image, dst);
            break;

        case 0x8004:
            EncodeImageDataI4(image, dst);
            break;

        case 0x8008:
            EncodeImageDataI8(image, dst);
            break;

        default:
            std::cout << "Invalid CI pixel size " << pixSize << std::endl;
            exit(1);
            break;
    }
}

void ParseAnimDataXml(std::string in_xml, AnimData* data)
{
    std::vector< ImageData> images;
    tinyxml2::XMLDocument document;
    std::string xmlDir = in_xml.substr(0, in_xml.find_last_of("\\/") + 1);
    XMLCheck(document.LoadFile(in_xml.c_str()));
    tinyxml2::XMLElement* root = document.FirstChildElement("animdata");
    if (!root) {
        std::cout << "Invalid Animation Data file." << std::endl;
        exit(1);
    }
    tinyxml2::XMLElement* child_elem;
    child_elem = root->FirstChildElement("pattern");
    while (child_elem) {
        ImageData image;
        const char* imgPath;
        uint8_t* rawData;
        int comp;
        std::string finalPath = xmlDir;
        XMLCheck(child_elem->QueryAttribute("center_x", &image.centerX));
        XMLCheck(child_elem->QueryAttribute("center_y", &image.centerY));
        XMLCheck(child_elem->QueryAttribute("path", &imgPath));
        finalPath += imgPath;
        rawData = stbi_load(finalPath.c_str(), &image.sizeX, &image.sizeY, &comp, 4);
        if (rawData == NULL) {
            std::cout << "Image loading failed because of " << stbi_failure_reason() << std::endl;
            exit(1);
        }
        image.dataRaw.resize(image.sizeX * image.sizeY * 4);
        memcpy(&image.dataRaw[0], rawData, image.sizeX * image.sizeY * 4);
        stbi_image_free(rawData);
        images.push_back(image);
        child_elem = child_elem->NextSiblingElement("pattern");
    }
    const char* texFmtName;
    XMLCheck(root->QueryAttribute("texfmt", &texFmtName));
    data->pixSize = GetTexFmtPixSize(texFmtName);
    bool hasPalette = (data->pixSize == 4) || (data->pixSize == 8);
    if (hasPalette) {
        int numColors;
        uint8_t palette[1024];
        exq_data* pExq = exq_init();
        pExq->numBitsPerChannel = 5;
        if (data->pixSize == 4) {
            numColors = 16;
        } else {
            numColors = 255;
        }
        for (auto& image : images) {
            exq_feed(pExq, &image.dataRaw[0], image.sizeX * image.sizeY);
        }
        exq_quantize_hq(pExq, numColors);
        exq_get_palette(pExq, palette, numColors);
        for (auto& image : images) {
            size_t numPixels = image.sizeX * image.sizeY;
            image.dataQuant.resize(numPixels);
            exq_map_image(pExq, numPixels, &image.dataRaw[0], &image.dataQuant[0]);
        }
        exq_free(pExq);
        data->palData.resize(numColors * 2);
        for (size_t i = 0; i < numColors; i++) {
            EncodeValueRGBA16(&palette[(i * 4)], &data->palData[(i * 2)]);
        }
    }
    child_elem = root->FirstChildElement("bank");
    while (child_elem) {
        std::vector<AnimFrame> bank;
        tinyxml2::XMLElement* frameElem = child_elem->FirstChildElement("frame");
        while (frameElem) {
            AnimFrame frame;
            unsigned int pat;
            int time;
            const char* flipName;
            frame.shiftX = frame.shiftY = 0;
            XMLCheck(frameElem->QueryAttribute("pattern", &pat));
            XMLCheck(frameElem->QueryAttribute("time", &time));
            XMLCheck(frameElem->QueryAttribute("flip", &flipName));
            frame.pat = pat;
            frame.time = time;
            frame.flip = GetFlipId(flipName);
            bank.push_back(frame);
            frameElem = frameElem->NextSiblingElement("frame");
        }
        //Write terminator frame
        if (bank.size() > 1) {
            AnimFrame frame;
            frame.pat = 0;
            frame.time = -1;
            frame.shiftX = frame.shiftY = 0;
            frame.flip = 0;
            bank.push_back(frame);
        }
        child_elem = child_elem->NextSiblingElement("bank");
        data->bank.push_back(bank);
    }
    for (auto& image : images) {
        AnimPattern pattern;
        pattern.centerX = image.centerX;
        pattern.centerY = image.centerY;
        pattern.sizeX = image.sizeX;
        pattern.sizeY = image.sizeY;
        EncodeImageData(data->pixSize, image, pattern.data);
        data->pattern.push_back(pattern);
    }
    data->srcW = data->srcH = 1;
}

void WriteAnimHeader(FILE* file, AnimDataFile* header)
{
    WriteU32(file, header->patternOfs);
    WriteU32(file, header->bankListOfs);
    WriteU32(file, header->imgDataOfs);
    WriteU32(file, header->palDataOfs);
    WriteU16(file, header->patCnt);
    WriteU16(file, header->bankCnt);
    WriteU16(file, header->srcW);
    WriteU16(file, header->srcH);
    WriteU16(file, header->pixSize);
    WriteU8(file, header->palCnt);
    WriteU8(file, 0);
    WriteU32(file, 0);
}

void WriteAnimData(std::string out_file, AnimData* data)
{
    FILE* file = fopen(out_file.c_str(), "wb");
    if (!file) {
        std::cout << "Failed to open " << out_file << " for writing." << std::endl;
        exit(1);
    }
    AnimDataFile fileData;
    fileData.bankCnt = data->bank.size();
    fileData.patCnt = data->pattern.size();
    fileData.palCnt = data->palData.size() / 2;
    fileData.srcW = data->srcW;
    fileData.srcH = data->srcH;
    fileData.pixSize = data->pixSize;
    fileData.patternOfs = 32;
    fileData.bankListOfs = fileData.patternOfs + (fileData.patCnt * sizeof(AnimPatternFile));
    fileData.imgDataOfs = fileData.bankListOfs + (sizeof(uint32_t) * fileData.bankCnt);
    for (size_t i = 0; i < fileData.bankCnt; i++) {
        fileData.imgDataOfs += sizeof(uint16_t);
        fileData.imgDataOfs += 7 * data->bank[i].size();
    }
    fileData.palDataOfs = fileData.imgDataOfs;
    for (size_t i = 0; i < fileData.patCnt; i++) {
        fileData.palDataOfs += data->pattern[i].data.size();
    }
    WriteAnimHeader(file, &fileData);
    size_t imgDataOfs = fileData.imgDataOfs;
    for (size_t i = 0; i < fileData.patCnt; i++) {
        WriteU32(file, imgDataOfs);
        WriteU16(file, data->pattern[i].sizeX);
        WriteU16(file, data->pattern[i].sizeY);
        WriteU16(file, data->pattern[i].centerX);
        WriteU16(file, data->pattern[i].centerY);
        imgDataOfs += data->pattern[i].data.size();
    }
    uint32_t bankOfs = fileData.bankListOfs + (sizeof(uint32_t) * fileData.bankCnt);
    for (size_t i = 0; i < fileData.bankCnt; i++) {
        WriteU32(file, bankOfs);
        bankOfs += sizeof(uint16_t);
        bankOfs += 7 * data->bank[i].size();
    }
    for (size_t i = 0; i < fileData.bankCnt; i++) {
        WriteU16(file, data->bank[i].size());
        for (size_t j = 0; j < data->bank[i].size(); j++) {
            AnimFrame& frame = data->bank[i][j];
            WriteU16(file, frame.pat);
            WriteS16(file, frame.time);
            WriteU8(file, frame.shiftX);
            WriteU8(file, frame.shiftY);
            WriteU8(file, frame.flip);
        }
    }
    for (size_t i = 0; i < fileData.patCnt; i++) {
        fwrite(&data->pattern[i].data[0], 1, data->pattern[i].data.size(), file);
    }
    if (data->pixSize == 4 || data->pixSize == 8) {
        fwrite(&data->palData[0], 1, data->palData.size(), file);
    }
    fclose(file);
}
void MakeAnimFile(std::string in_xml, std::string out_file)
{
    AnimData animData;
    ParseAnimDataXml(in_xml, &animData);
    WriteAnimData(out_file, &animData);
}
int main(int argc, char **argv)
{
    std::string mode;
    std::string in, out;
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " mode in out" << std::endl;
        std::cout << "The mode parameter should be either dump or rebuild." << std::endl;
        return 1;
    }
    mode = argv[1];
    in = argv[2];
    out = argv[3];
    if (mode == "dump") {
        DumpAnimFile(in, out);
    } else if (mode == "build") {
        MakeAnimFile(in, out);
    } else {
        std::cout << "Invalid mode parameter " << mode << std::endl;
    }
}
