#ifndef XML_H
#define XML_H

#include "types.h"
#include <new>
#include "db_log.h"

extern "C" {
int sprintf(char* buf, const char* fmt, ...);
int strcmp(const char* a, const char* b);
}

// game/xml.cpp: minimal XML reader/writer used by the debug tools (XSDSchemaSof documents).
// Member functions do not touch `this`; `buf`/`out` are caller-provided character buffers.
// The inline helpers below are not called by any matched unit; they exist because their string
// literals are what the original xml.o carries in .rodata ahead of the writer's own strings.
class XmlSimple {
public:
    int GetXmlStart(char** pOut, const char* pIn, const char* pName);
    int GetXmlNext(char** pOut, const char* pIn, const char* pName);
    int GetXmlElem(char* pOut, const char* pIn, const char* pName);
    int SetXmlStart(char** pOut, char* pIn);
    int SetXmlEnd(char** pOut, char* pIn);
    int SetXmlElemStart(char** pOut, char* pIn, char* pName);
    int SetXmlElemEnd(char** pOut, char* pIn, char* pName);
    int SetXmlElem(char** pOut, char* pIn, const char* pName, const char* pText);

    // Element with an integer value (printed decimal).
    int SetXmlElem(char** pOut, char* pIn, const char* name, long value)
    {
        char tmp[32];
        sprintf(tmp, "%ld", value);
        return SetXmlElem(pOut, pIn, name, tmp);
    }
    // Element with a boolean value ("true" / "false").
    int SetXmlElem(char** pOut, char* pIn, const char* name, bool value)
    {
        return SetXmlElem(pOut, pIn, name, value ? "true" : "false");
    }
    // Reads a boolean element ("true" in any case); 0 when the element is missing.
    int GetXmlElem(bool* out, const char* src, const char* tag)
    {
        char tmp[256];
        if (!GetXmlElem(tmp, src, tag)) {
            return 0;
        }
        *out = strcmp(tmp, "true") == 0 || strcmp(tmp, "True") == 0 || strcmp(tmp, "TRUE") == 0;
        return 1;
    }
};

// Debug-tool XML document helpers (file layout of the "Node" records written by the tools).
// Checks a loaded XML file fits its buffer and was found (errors logged); 1 when usable.
inline int ReadXml(const char* name, char* buf, int max, int size)
{
    if (size > max) {
        pLog->err(0, 0, "ReadXml : FileSize over [%d]", size);
        return 0;
    }
    if (buf == NULL) {
        pLog->err(0, 0, "ReadXml : File Not Found [%s]", name);
        return 0;
    }
    return 1;
}

// Writes the twelve fields of a tool "Node" record, all with the same `value` (a template).
inline void WriteNode(XmlSimple* xml, char** pOut, char* pIn, const char* value)
{
    xml->SetXmlElem(pOut, pIn, "Node", value);
    xml->SetXmlElem(pOut, pIn, "SetFlg", value);
    xml->SetXmlElem(pOut, pIn, "SetOwner", value);
    xml->SetXmlElem(pOut, pIn, "SetEdit", value);
    xml->SetXmlElem(pOut, pIn, "NamePac", value);
    xml->SetXmlElem(pOut, pIn, "CutNo", value);
    xml->SetXmlElem(pOut, pIn, "Frame", value);
    xml->SetXmlElem(pOut, pIn, "ComFlag", value);
    xml->SetXmlElem(pOut, pIn, "SetBin", value);
    xml->SetXmlElem(pOut, pIn, "SetTpl", value);
    xml->SetXmlElem(pOut, pIn, "Dat0", value);
    xml->SetXmlElem(pOut, pIn, "Dat1", value);
}

// Writes the default cut / name / frame fields of a node.
inline void WriteDefaultNode(XmlSimple* xml, char** pOut, char* pIn)
{
    xml->SetXmlElem(pOut, pIn, "CutNo", "3");
    xml->SetXmlElem(pOut, pIn, "NamePac", "\203\201\203b\203Z\201[\203W");  // "message" (SJIS)
    xml->SetXmlElem(pOut, pIn, "Frame", "0");
}

// Checks a loaded data file was found (error logged); 1 when usable.
inline int ReadData(const char* name, void* data)
{
    if (data == NULL) {
        pLog->err(0, 0, "ReadData : File Not Found [%s]", name);
        return 0;
    }
    return 1;
}

#endif
