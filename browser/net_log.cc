// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "browser/net_log.h"

#include "browser/browser_context.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "net/log/net_log_util.h"
#include "net/url_request/url_request_context.h"

namespace {

scoped_ptr<base::DictionaryValue> GetConstants() {
  scoped_ptr<base::DictionaryValue> constants = net::GetNetConstants();

  // Adding client information to constants dictionary.
  base::DictionaryValue* client_info = new base::DictionaryValue();
  client_info->SetString(
      "command_line",
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());

  constants->Set("clientInfo", client_info);
  return constants;
}

}  // namespace

namespace brightray {

NetLog::NetLog(net::URLRequestContext* context)
    : added_events_(false),
      context_(context) {
  auto command_line = base::CommandLine::ForCurrentProcess();
  base::FilePath log_path =
      command_line->GetSwitchValuePath(switches::kLogNetLog);

  #if defined(OS_WIN)
    log_file_.reset(_wfopen(log_path.value().c_str(), L"w"));
  #elif defined(OS_POSIX)
    log_file_.reset(fopen(log_path.value().c_str(), "w"));
  #endif

  if (!log_file_) {
    LOG(ERROR) << "Could not open file: " << log_path.value()
               << "for net logging";
  } else {
    std::string json;
    scoped_ptr<base::Value> constants(GetConstants().Pass());
    base::JSONWriter::Write(constants.get(), &json);
    fprintf(log_file_.get(), "{\"constants\": %s, \n", json.c_str());
    fprintf(log_file_.get(), "\"events\": [\n");

    if (context_) {
      DCHECK(context_->CalledOnValidThread());

      std::set<net::URLRequestContext*> contexts;
      contexts.insert(context_);

      net::CreateNetLogEntriesForActiveObjects(contexts, this);
    }

    DeprecatedAddObserver(this, net::NetLog::LogLevel::LOG_STRIP_PRIVATE_DATA);
  }
}

NetLog::~NetLog() {
  DeprecatedRemoveObserver(this);

  // Ending events array.
  fprintf(log_file_.get(), "]}");
  log_file_.reset();
}

void NetLog::OnAddEntry(const net::NetLog::Entry& entry) {
  std::string json;
  base::JSONWriter::Write(entry.ToValue(), &json);

  fprintf(log_file_.get(), "%s%s", (added_events_ ? ",\n" : ""), json.c_str());
  added_events_ = true;
}

}  // namespace brightray
