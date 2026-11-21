#pragma once
#include "sources.h"
#include "providers.h"
enum class DownloadState {Queued, Active, Pausing, Paused, Complete, Failed, Cancelled};
inline const char* stateName(DownloadState s){switch(s){case DownloadState::Queued:return "Queued";case DownloadState::Active:return "Downloading";case DownloadState::Pausing:return "Pausing";case DownloadState::Paused:return "Paused";case DownloadState::Complete:return "Complete";case DownloadState::Failed:return "Failed";default:return "Cancelled";}}
inline bool isDiagnosticTorrent(const std::string&url){return url=="https://webtorrent.io/torrents/big-buck-bunny.torrent";}
struct DownloadRow {int id=0,gameId=0;std::string title,url,page,phase,error,infoHash,outputDir;bool removed=false,deleteFiles=false;DownloadState state=DownloadState::Queued;uint64_t done=0,total=0;double rate=0;unsigned peers=0;};
inline std::string bytesText(uint64_t n){const char*u[]={"B","KB","MB","GB","TB"};double value=n;int i=0;while(value>=1024&&i<4){value/=1024;++i;}char s[48];snprintf(s,sizeof s,i?"%.1f %s":"%.0f %s",value,u[i]);return s;}
