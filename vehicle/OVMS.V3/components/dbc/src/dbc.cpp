/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_log.h"
static const char *TAG = "dbc";

#include <algorithm>
#include <list>
#include <vector>
#include "dbc.h"
#include "ovms_config.h"

dbc MyDBC __attribute__ ((init_priority (4510)));

////////////////////////////////////////////////////////////////////////
// dbcComment...

dbcCommentTable::dbcCommentTable()
  {
  }

dbcCommentTable::~dbcCommentTable()
  {
  EmptyContent();
  }

void dbcCommentTable::AddComment(std::string comment)
  {
  m_entrymap.push_back(comment);
  }

void dbcCommentTable::RemoveComment(std::string comment)
  {
  m_entrymap.remove(comment);
  }

bool dbcCommentTable::HasComment(std::string comment)
  {
  auto it = std::find(m_entrymap.begin(), m_entrymap.end(), comment);
  return (it != m_entrymap.end());
  }

void dbcCommentTable::EmptyContent()
  {
  m_entrymap.clear();
  }

void dbcCommentTable::ReplaceContent(dbcCommentTable* source)
  {
  EmptyContent();
  for (std::string comment : source->m_entrymap)
    {
    m_entrymap.push_back(comment);
    }
  source->m_entrymap.clear();
  }

////////////////////////////////////////////////////////////////////////
// dbcNewSymbol...

dbcNewSymbolTable::dbcNewSymbolTable()
  {
  }

dbcNewSymbolTable::~dbcNewSymbolTable()
  {
  EmptyContent();
  }

void dbcNewSymbolTable::AddSymbol(std::string symbol)
  {
  m_entrymap.push_back(symbol);
  }

void dbcNewSymbolTable::RemoveSymbol(std::string symbol)
  {
  m_entrymap.remove(symbol);
  }

bool dbcNewSymbolTable::HasSymbol(std::string symbol)
  {
  auto it = std::find(m_entrymap.begin(), m_entrymap.end(), symbol);
  return (it != m_entrymap.end());
  }

void dbcNewSymbolTable::EmptyContent()
  {
  m_entrymap.clear();
  }

void dbcNewSymbolTable::ReplaceContent(dbcNewSymbolTable* source)
  {
  EmptyContent();
  for (std::string ns : source->m_entrymap)
    {
    m_entrymap.push_back(ns);
    }
  source->m_entrymap.clear();
  }

////////////////////////////////////////////////////////////////////////
// dbcNode...

dbcNode::dbcNode()
  {
  }

dbcNode::dbcNode(std::string name)
  {
  m_name = name;
  }

dbcNode::~dbcNode()
  {
  }

void dbcNode::AddComment(std::string comment)
  {
  m_comments.AddComment(comment);
  }

void dbcNode::RemoveComment(std::string comment)
  {
  m_comments.RemoveComment(comment);
  }

bool dbcNode::HasComment(std::string comment)
  {
  return m_comments.HasComment(comment);
  }

dbcNodeTable::dbcNodeTable()
  {
  }

dbcNodeTable::~dbcNodeTable()
  {
  EmptyContent();
  }

void dbcNodeTable::AddNode(dbcNode* node)
  {
  m_entrymap.push_back(node);
  }

void dbcNodeTable::RemoveNode(dbcNode* node, bool free)
  {
  m_entrymap.remove(node);
  if (free) delete node;
  }

dbcNode* dbcNodeTable::FindNode(std::string name)
  {
  for (dbcNode* node : m_entrymap)
    {
    if (node->m_name.compare(name)==0) return node;
    }
  return NULL;
  }

void dbcNodeTable::EmptyContent()
  {
  for (dbcNode* node : m_entrymap)
    {
    delete node;
    }
  m_entrymap.clear();
  }

void dbcNodeTable::ReplaceContent(dbcNodeTable* source)
  {
  EmptyContent();
  for (dbcNode* node : source->m_entrymap)
    {
    m_entrymap.push_back(node);
    }
  source->m_entrymap.clear();
  }

////////////////////////////////////////////////////////////////////////
// dbcBitTiming...

dbcBitTiming::dbcBitTiming()
  {
  }

dbcBitTiming::~dbcBitTiming()
  {
  m_baudrate = 0;
  m_btr1 = 0;
  m_btr2 = 0;
  }

void dbcBitTiming::EmptyContent()
  {
  }

void dbcBitTiming::ReplaceContent(dbcBitTiming* source)
  {
  m_baudrate = source->m_baudrate;
  m_btr1 = source->m_btr1;
  m_btr2 = source->m_btr2;
  source->EmptyContent();
  }

////////////////////////////////////////////////////////////////////////
// dbcValueTable...

dbcValueTable::dbcValueTable()
  {
  }

dbcValueTable::dbcValueTable(std::string name)
  {
  m_name = name;
  }

void dbcValueTable::AddValue(uint32_t id, std::string value)
  {
  m_entrymap[id] = value;
  }

void dbcValueTable::RemoveValue(uint32_t id)
  {
  auto search = m_entrymap.find(id);
  if (search != m_entrymap.end())
    m_entrymap.erase(search);
  }

bool dbcValueTable::HasValue(uint32_t id)
  {
  auto search = m_entrymap.find(id);
  return (search != m_entrymap.end());
  }

std::string dbcValueTable::GetValue(uint32_t id)
  {
  auto search = m_entrymap.find(id);
  if (search != m_entrymap.end())
    return search->second;
  else
    return std::string("");
  }

dbcValueTable::~dbcValueTable()
  {
  EmptyContent();
  }

void dbcValueTable::EmptyContent()
  {
  m_entrymap.clear();
  }

void dbcValueTable::ReplaceContent(dbcValueTable* source)
  {
  dbcValueTableEntry_t::iterator it=m_entrymap.begin();
  while (it!=m_entrymap.end())
    {
    m_entrymap[it->first] = it->second;
    ++it;
    }
  source->m_entrymap.clear();
  }

dbcValueTableTable::dbcValueTableTable()
  {
  }

dbcValueTableTable::~dbcValueTableTable()
  {
  EmptyContent();
  }

void dbcValueTableTable::AddValueTable(std::string name, dbcValueTable* vt)
  {
  m_entrymap[name] = vt;
  }

void dbcValueTableTable::RemoveValueTable(std::string name, bool free)
  {
  auto search = m_entrymap.find(name);
  if (search != m_entrymap.end())
    {
    if (free) delete search->second;
    m_entrymap.erase(search);
    }
  }

dbcValueTable* dbcValueTableTable::FindValueTable(std::string name)
  {
  auto search = m_entrymap.find(name);
  if (search != m_entrymap.end())
    return search->second;
  else
    return NULL;
  }

void dbcValueTableTable::EmptyContent()
  {
  dbcValueTableTableEntry_t::iterator it=m_entrymap.begin();
  while (it!=m_entrymap.end())
    {
    delete it->second;
    ++it;
    }
  m_entrymap.clear();
  }

void dbcValueTableTable::ReplaceContent(dbcValueTableTable* source)
  {
  dbcValueTableTableEntry_t::iterator it=m_entrymap.begin();
  while (it!=m_entrymap.end())
    {
    m_entrymap[it->first] = it->second;
    ++it;
    }
  source->m_entrymap.clear();
  }

////////////////////////////////////////////////////////////////////////
// dbcSignal

dbcSignal::dbcSignal()
  {
  }

dbcSignal::~dbcSignal()
  {
  }

void dbcSignal::AddReceiver(std::string receiver)
  {
  m_receivers.push_back(receiver);
  }

void dbcSignal::RemoveReceiver(std::string receiver)
  {
  m_receivers.remove(receiver);
  }

bool dbcSignal::HasReceiver(std::string receiver)
  {
  auto it = std::find(m_receivers.begin(), m_receivers.end(), receiver);
  return (it != m_receivers.end());
  }

void dbcSignal::AddComment(std::string comment)
  {
  m_comments.push_back(comment);
  }

void dbcSignal::RemoveComment(std::string comment)
  {
  m_comments.remove(comment);
  }

bool dbcSignal::HasComment(std::string comment)
  {
  auto it = std::find(m_comments.begin(), m_comments.end(), comment);
  return (it != m_comments.end());
  }

////////////////////////////////////////////////////////////////////////
// dbcMessage...

dbcMessage::dbcMessage()
  {
  }

dbcMessage::dbcMessage(uint32_t id)
  {
  m_id = id;
  }

dbcMessage::~dbcMessage()
  {
  }

void dbcMessage::AddComment(std::string comment)
  {
  m_comments.push_back(comment);
  }

void dbcMessage::RemoveComment(std::string comment)
  {
  m_comments.remove(comment);
  }

bool dbcMessage::HasComment(std::string comment)
  {
  auto it = std::find(m_comments.begin(), m_comments.end(), comment);
  return (it != m_comments.end());
  }

void dbcMessage::AddSignal(dbcSignal* signal)
  {
  m_signals.push_back(signal);
  }

void dbcMessage::RemoveSignal(dbcSignal* signal, bool free)
  {
  m_signals.remove(signal);
  if (free) delete signal;
  }

dbcSignal* dbcMessage::FindSignal(std::string name)
  {
  for (dbcSignal* signal : m_signals)
    {
    if (signal->m_name.compare(name)==0) return signal;
    }
  return NULL;
    }

dbcMessageTable::dbcMessageTable()
  {
  }

dbcMessageTable::~dbcMessageTable()
  {
  EmptyContent();
  }

void dbcMessageTable::AddMessage(uint32_t id, dbcMessage* message)
  {
  m_entrymap[id] = message;
  }

void dbcMessageTable::RemoveMessage(uint32_t id, bool free)
  {
  auto search = m_entrymap.find(id);
  if (search != m_entrymap.end())
    {
    if (free) delete search->second;
    m_entrymap.erase(search);
    }
  }

dbcMessage* dbcMessageTable::FindMessage(uint32_t id)
  {
  auto search = m_entrymap.find(id);
  if (search != m_entrymap.end())
    return search->second;
  else
    return NULL;
  }

void dbcMessageTable::EmptyContent()
  {
  dbcMessageEntry_t::iterator it=m_entrymap.begin();
  while (it!=m_entrymap.end())
    {
    delete it->second;
    ++it;
    }
  m_entrymap.clear();
  }

void dbcMessageTable::ReplaceContent(dbcMessageTable* source)
  {
  dbcMessageEntry_t::iterator it=m_entrymap.begin();
  while (it!=m_entrymap.end())
    {
    m_entrymap[it->first] = it->second;
    delete it->second;
    ++it;
    }
  source->m_entrymap.clear();
  }

////////////////////////////////////////////////////////////////////////
// dbcfile

dbcfile::dbcfile()
  {
  }

dbcfile::~dbcfile()
  {
  FreeAllocations();
  }

bool dbcfile::LoadParseOneLine(int linenumber, std::string line)
  {
  std::vector<std::string> tokens;

  int len = line.length();
  bool inquote = false;
  int arglen;
  for (int k = 0; k < len; k++)
    {
    int start = k;
    if ((k==0)&&(line[k]==' '))
      {
      while (k<len && line[k]==' ')
        {
        k++;
        start++;
        }
      }
    if (line[k] == '\"') inquote = true;
    if (inquote)
      {
      k++;
      start++;
      while (k<len && line[k] != '\"')
        k++;
      if (k<len)
        inquote = false;
      arglen = k-start;
      k++;
      }
    else
      {
      while (k<len && line[k]!=' ')
        k++;
      arglen = k-start;
      if ((arglen>1)&&(line[start+arglen-1]==':'))
        {
        // Split off trailing ':' as a separate token
        tokens.push_back(line.substr(start, arglen-1));
        start = k-1;
        arglen = 1;
        }
      }
    tokens.push_back(line.substr(start, arglen));
    }
 if (inquote)
   {
   ESP_LOGW(TAG,"Open quote (line #%d)", linenumber);
   return false;
   }

  if (tokens.size() == 0) return true; // Skip blank lines

  //for (int k=0;k<tokens.size();k++)
  ///  ESP_LOGD(TAG,"Tokensize line#%d %d=%s",linenumber,k,tokens[k].c_str());

  std::string keyword = tokens.front();
  if (keyword.compare("VERSION")==0)
    {
    if (tokens.size()>=2)
      m_version = tokens[1];
    }
  else if (keyword.compare("NS_")==0)
    {
    if ((tokens.size()>=3)&&(tokens[1].compare(":")==0))
      {
      for (int k=2; k<tokens.size(); k++)
        {
        m_newsymbols.m_entrymap.push_back(tokens[k]);
        }
      }
    else
      {
      ESP_LOGW(TAG,"Syntax error in NS_ (line #%d)", linenumber);
      return false;
      }
    }
  else if (keyword.compare("BS_")==0)
    {
    // TODO: For the moment, ignore this
    ESP_LOGW(TAG,"Ignoring BS_ statement (line #%d)", linenumber);
    }
  else if (keyword.compare("BU_")==0)
    {
    if ((tokens.size()>=3)&&(tokens[1].compare(":")==0))
      {
      for (int k=2; k<tokens.size(); k++)
        {
        dbcNode* node = new dbcNode(tokens[k]);
        m_nodes.m_entrymap.push_back(node);
        }
      }
    else
      {
      ESP_LOGW(TAG,"Syntax error in BU_ (line #%d)", linenumber);
      return false;
      }
    }
  else if (keyword.compare("VAL_TABLE_")==0)
    {
    if (tokens.size()>=4)
      {
      dbcValueTable* vt = new dbcValueTable(tokens[1]);
      m_values.m_entrymap[tokens[1]] = vt;
      for (int k=2; k<(tokens.size()-1); k+=2)
        {
        vt->m_entrymap[atoi(tokens[k].c_str())] = tokens[k+1];
        }
      }
    else
      {
      ESP_LOGW(TAG,"Syntax error in VAL_TABLE_ (line #%d)", linenumber);
      return false;
      }
    }
  else if (keyword.compare("BO_")==0)
    {
    if (tokens.size()==6)
      {
      uint32_t id = atoi(tokens[1].c_str());
      dbcMessage* m = new dbcMessage(id);
      m->m_name = tokens[2];
      m->m_size = atoi(tokens[4].c_str());
      m->m_transmitter_node = tokens[5];
      m_messages.m_entrymap[id] = m;
      m_lastmsg = m;
      }
    else
      {
      ESP_LOGW(TAG,"Syntax error in BO_ (line #%d)", linenumber);
      return false;
      }
    }
  else
    {
    // Ignore unrecognised lines
    ESP_LOGW(TAG,"Unrecognised token (line #%d): %s", linenumber,keyword.c_str());
    for (int k=0;k<tokens.size();k++)
      ESP_LOGW(TAG,"  line#%d %d=%s",linenumber,k,tokens[k].c_str());
    }

  return true;
  }

void dbcfile::FreeAllocations()
  {
  m_version.clear();
  m_newsymbols.EmptyContent();
  m_bittiming.EmptyContent();
  m_nodes.EmptyContent();
  m_values.EmptyContent();
  m_messages.EmptyContent();
  m_comments.EmptyContent();
  }

bool dbcfile::LoadFile(const char* path)
  {
  if (MyConfig.ProtectedPath(path))
    {
    ESP_LOGW(TAG,"Path %s is protected",path);
    return false;
    }

  FILE* f = fopen(path, "r");
  if (!f)
    {
    ESP_LOGW(TAG,"Could not open %s for reading",path);
    return false;
    }

  m_lastmsg = NULL;
  char* buf = new char[DBC_MAX_LINELENGTH];
  bool result = true;
  int linenumber=0;
  while (fgets(buf, DBC_MAX_LINELENGTH, f))
    {
    linenumber++;
    int len = strlen(buf);
    if ((len>0)&&(buf[len-1]=='\n')) buf[len-1]=0; // Remove trailing LF
    len = strlen(buf);
    if ((len>0)&&(buf[len-1]=='\r')) buf[len-1]=0; // Remove trailing CR
    if (!LoadParseOneLine(linenumber,buf)) result=false;
    }
  delete [] buf;

  if (!result)
    ESP_LOGW(TAG,"File %s only partially loaded (syntax errors)",path);

  return result;
  }

bool dbcfile::LoadString(const char* source)
  {
  return false;
  }

void dbcfile::ReplaceContent(dbcfile* dbc)
  {
  m_version = dbc->m_version;
  m_newsymbols.ReplaceContent(&dbc->m_newsymbols);
  m_bittiming.ReplaceContent(&dbc->m_bittiming);
  m_nodes.ReplaceContent(&dbc->m_nodes);
  m_values.ReplaceContent(&dbc->m_values);
  m_messages.ReplaceContent(&dbc->m_messages);
  m_comments.ReplaceContent(&dbc->m_comments);

  delete dbc;
  }

void dbcfile::ShowStatusLine(OvmsWriter* writer)
  {
  writer->printf("%d symbols, %d node(s), %d vt(s), %d message(s)",
    m_newsymbols.m_entrymap.size(),
    m_nodes.m_entrymap.size(),
    m_values.m_entrymap.size(),
    m_messages.m_entrymap.size());

  if (!m_version.empty())
    writer->printf(" (version %s)",m_version.c_str());
  }

////////////////////////////////////////////////////////////////////////
// dbc

void dbc_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsMutexLock ldbc(&MyDBC.m_mutex);

  dbcLoadedFiles_t::iterator it=MyDBC.m_dbclist.begin();
  while (it!=MyDBC.m_dbclist.end())
    {
    writer->printf("%s: ",it->first.c_str());
    dbcfile* dbcf = it->second;
    dbcf->ShowStatusLine(writer);
    writer->puts("");
    ++it;
    }
  }

void dbc_load(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.LoadFile(argv[0],argv[1]))
    {
    writer->printf("Loaded DBC %s ok\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Failed to load DBC %s from %s\n",argv[0],argv[1]);
    }
  }

void dbc_unload(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.Unload(argv[0]))
    {
    writer->printf("Unloaded DBC %s ok\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Failed to unload DBC %s\n",argv[0]);
    }
  }

dbc::dbc()
  {
  ESP_LOGI(TAG, "Initialising DBC (4510)");

  OvmsCommand* cmd_dbc = MyCommandApp.RegisterCommand("dbc","DBC framework",NULL, "", 0, 0, true);

  cmd_dbc->RegisterCommand("list", "List DBC status", dbc_list, "", 0, 0, true);
  cmd_dbc->RegisterCommand("load", "Load DBC file", dbc_load, "<name> <path>", 2, 2, true);
  cmd_dbc->RegisterCommand("unload", "Unload DBC file", dbc_unload, "<name>", 1, 1, true);
  }

dbc::~dbc()
  {
  }

bool dbc::LoadFile(const char* name, const char* path)
  {
  OvmsMutexLock ldbc(&m_mutex);

  dbcfile* ndbc = new dbcfile();
  if (!ndbc->LoadFile(path))
    {
//    delete ndbc;
//    return false;
    }

  auto k = m_dbclist.find(name);
  if (k == m_dbclist.end())
    {
    // Create a new entry...
    m_dbclist[name] = ndbc;
    }
  else
    {
    // Replace it inline...
    k->second->ReplaceContent(ndbc);
    }

  return true;
  }

bool dbc::LoadString(const char* name, const char* source)
  {
  OvmsMutexLock ldbc(&m_mutex);

  dbcfile* ndbc = new dbcfile();
  if (!ndbc->LoadString(source))
    {
    delete ndbc;
    return false;
    }

  auto k = m_dbclist.find(name);
  if (k == m_dbclist.end())
    {
    // Create a new entry...
    m_dbclist[name] = ndbc;
    }
  else
    {
    // Replace it inline...
    k->second->ReplaceContent(ndbc);
    }

  return true;
  }

bool dbc::Unload(const char* name)
  {
  OvmsMutexLock ldbc(&m_mutex);

  return false;
  }

dbcfile* dbc::Find(const char* name)
  {
  OvmsMutexLock ldbc(&m_mutex);

  auto k = m_dbclist.find(name);
  if (k == m_dbclist.end())
    return NULL;
  else
    return k->second;
  }
