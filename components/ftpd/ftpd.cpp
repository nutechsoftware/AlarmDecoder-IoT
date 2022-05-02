/**
*  @file    ftpd.cpp
*  @author  Sean Mathews <coder@f34r.com>
*  @date    03/5/2022
*  @version 1.0.0
*
*  @brief Simple ftp daemon for updates to uSD on AD2IoT
*  based upon work by Kolban. If you don't have his book get it. I liked
*  it so much I purchased his precisely organized electrons twice!
*  https://github.com/nkolban/esp32-snippets/tree/master/networking/FTPServer
*
*  @copyright Copyright (C) 2021 Nu Tech Software Solutions, Inc.
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
*/
// AlarmDecoder std includes
#include "alarmdecoder_main.h"

// Disable via config
#if CONFIG_AD2IOT_FTP_DAEMON

static const char *TAG = "FTPD";

// module specific includes
#include <dirent.h>
#include <fstream>

#define FTPD_COMMAND          "ftpd"
#define FTPD_SUBCMD_ENABLE    "enable"
#define FTPD_SUBCMD_ACL       "acl"

#define FTPD_CONFIG_SECTION   "ftpd"

// enable verbose debug logging
//#define FTPD_DEBUG

/**
 * ftpd command list and enum.
 */
char * FTPD_SUBCMD [] = {
    (char*)FTPD_SUBCMD_ENABLE,
    (char*)FTPD_SUBCMD_ACL,
    0 // EOF
};

enum {
    FTPD_SUBCMD_ENABLE_ID = 0,
    FTPD_SUBCMD_ACL_ID,
};

class FTPDCallbacks
{
public:
    virtual void        onStoreStart(std::string fileName);
    virtual size_t      onStoreData(uint8_t* data, size_t size);
    virtual void        onStoreEnd();
    virtual void        onRetrieveStart(std::string fileName);
    virtual size_t      onRetrieveData(uint8_t *data, size_t size);
    virtual void        onRetrieveEnd();
    virtual std::string onDir(std::string path);
    virtual ~FTPDCallbacks();
};

/**
 * An implementation of FTPDCallbacks that uses Posix File I/O to perform file access.
 */
class FTPDFileCallbacks : public FTPDCallbacks
{
private:
    std::ofstream m_storeFile;      // File used to store data from the client.
    std::ifstream m_retrieveFile;   // File used to retrieve data for the client.
    uint32_t      m_byteCount;      // Count of bytes sent over wire.

public:
    void        onStoreStart(std::string fileName) override;            // Called for a STOR request.
    size_t      onStoreData(uint8_t* data, size_t size) override;       // Called when a chunk of STOR data becomes available.
    void        onStoreEnd() override;                                  // Called at the end of a STOR request.
    void        onRetrieveStart(std::string fileName) override;         // Called at the start of a RETR request.
    size_t      onRetrieveData(uint8_t* data, size_t size) override;    // Called to retrieve a chunk of RETR data.
    void        onRetrieveEnd() override;                               // Called when we have retrieved all the data.
    std::string onDir(std::string path) override;                       // Called to retrieve all the directory entries.
};


class FTPD
{
private:
    int           m_serverSocket;   // The socket the FTP server is listening on.
    int           m_clientSocket;   // The current client socket.
    int           m_dataSocket;     // The data socket.
    int           m_passiveSocket;  // The socket on which the server is listening for passive FTP connections.
    uint16_t      m_port;           // The port the FTP server will use.
    uint16_t      m_dataPort;       // The port for data connections.
    uint32_t      m_dataIp;         // The ip address for data connections.
    bool          m_isPassive;      // Are we in passive mode?  If not, then we are in active mode.
    bool          m_isImage;        // Are we in image mode?
    size_t        m_chunkSize;      // The maximum chunk size.
    std::string   m_userid;         // The required userid.
    std::string   m_password;       // The required password.
    std::string   m_suppliedUserid; // The userid supplied from the USER command.
    bool          m_loginRequired;  // Do we required a login?
    bool          m_isAuthenticated;// Have we authenticated?
    std::string   m_lastCommand;    // The last command that was processed.
    std::string   m_save;           // Used to save stuff between commands such as RNFR and RNTO
    int           m_save_clear;     // counter to remove save
    FTPDCallbacks*    m_callbacks;  // The callbacks for processing.

    void closeConnection();
    void closeData();
    void closePassive();
    void onAuth(std::istringstream& ss);
    void onCwd(std::istringstream& ss);
    void onCdup(std::istringstream& ss);
    void onRnfr(std::istringstream& ss);
    void onRnto(std::istringstream& ss);
    void onList(std::istringstream& ss);
    void onMkd(std::istringstream& ss);
    void onNoop(std::istringstream& ss);
    void onPass(std::istringstream& ss);
    void onPasv(std::istringstream& ss);
    void onPort(std::istringstream& ss);
    void onDele(std::istringstream& ss);
    void onPWD(std::istringstream& ss);
    void onQuit(std::istringstream& ss);
    void onRetr(std::istringstream& ss);
    void onRmd(std::istringstream& ss);
    void onStor(std::istringstream& ss);
    void onSyst(std::istringstream& ss);
    void onType(std::istringstream& ss);
    void onUser(std::istringstream& ss);
    void onXmkd(std::istringstream& ss);
    void onXrmd(std::istringstream& ss);
    void onRest(std::istringstream& ss);
    bool openData();

    void receiveFile(std::string fileName);
    void sendResponse(int code);
    void sendResponse(int code, std::string text);
    void sendData(uint8_t* pData, uint32_t size);
    std::string listenPassive();
    int waitForFTPClient();
    void processCommand();
public:
    FTPD();
    virtual ~FTPD();
    void setCredentials(std::string userid, std::string password);
    void start();
    void setPort(uint16_t port);
    void setCallbacks(FTPDCallbacks* pFTPDCallbacks);
    class FileException: public std::exception
    {

    };

    // Response codes.
    static const int RESPONSE_150_ABOUT_TO_OPEN_DATA_CONNECTION = 150;
    static const int RESPONSE_200_COMMAND_OK                    = 200;
    static const int RESPONSE_202_COMMAND_NOT_IMPLEMENTED       = 202;
    static const int RESPONSE_212_DIRECTORY_STATUS              = 212;
    static const int RESPONSE_213_FILE_STATUS                   = 213;
    static const int RESPONSE_214_HELP_MESSAGE                  = 214;
    static const int RESPONSE_220_SERVICE_READY                 = 220;
    static const int RESPONSE_221_CLOSING_CONTROL_CONNECTION    = 221;
    static const int RESPONSE_230_USER_LOGGED_IN                = 230;
    static const int RESPONSE_226_CLOSING_DATA_CONNECTION       = 226;
    static const int RESPONSE_227_ENTERING_PASSIVE_MODE         = 227;
    static const int RESPONSE_331_PASSWORD_REQUIRED             = 331;
    static const int RESPONSE_332_NEED_ACCOUNT                  = 332;
    static const int RESPONSE_350_RESPONSE_CODE                 = 350;
    static const int RESPONSE_500_COMMAND_UNRECOGNIZED          = 500;
    static const int RESPONSE_502_COMMAND_NOT_IMPLEMENTED       = 502;
    static const int RESPONSE_503_BAD_SEQUENCE                  = 503;
    static const int RESPONSE_530_NOT_LOGGED_IN                 = 530;
    static const int RESPONSE_550_ACTION_NOT_TAKEN              = 550;
    static const int RESPONSE_553_FILE_NAME_NOT_ALLOWED         = 553;
};


/**
 * @brief globals
 *
 */
static FTPD *ad2ftpd = nullptr;
static ad2_acl_check ad2ftpd_acl;     // ACL control
static std::string ad2ftpd_cwd;       // current directory

/**
 * @brief format a file / path line response for LIST command.
 *
 */
std::string _format_dir_line(const char *filename, struct stat *fs)
{
    std::stringstream ss;
    if (S_ISDIR(fs->st_mode)) {
        // Directory
        ss << "d";
    } else {
        // File
        ss << "-";
    }

    // USER READ
    ss << (fs->st_mode & S_IRUSR ? "r" : "-");
    // USER WRITE
    ss << (fs->st_mode & S_IWUSR ? "w" : "-");
    // USER EXEC
    ss << (fs->st_mode & S_IXUSR ? "x" : "-");

    // GROUP READ
    ss << (fs->st_mode & S_IRGRP ? "r" : "-");
    // GROUP WRITE
    ss << (fs->st_mode & S_IWGRP ? "w" : "-");
    // GROUP EXEC
    ss << (fs->st_mode & S_IXGRP ? "x" : "-");

    // OTHER READ
    ss << (fs->st_mode & S_IROTH ? "r" : "-");
    // OTHER WRITE
    ss << (fs->st_mode & S_IWOTH ? "w" : "-");
    // OTHER EXEC
    ss << (fs->st_mode & S_IXOTH ? "x" : "-");
    // add fake user and group names...
    ss << " " << 1 << " ftp ftp " << fs->st_size;
    // format date
    char tt[100];
    strftime(tt, 100, "%b %d %H:%M", localtime(&fs->st_mtime));
    ss << " " << tt << " ";
    // filename
    ss << filename << "\r\n";

    return ss.str();
}

/**
 * @brief Fix the virtual and file path if realtive.
 * @param cwd[in] current virtual working directory
 * @param &path[in/out] relative or full virtual path
 * @param &tp[out] translated file system path
 */
void relative_path_fix(std::string cwd, std::string path, std::string &tp)
{
    std::string tpath = path;
    std::string ttp = tp;

    // special case -aL arg.
    //Fix better later for now strip and clear path.
    if (path[0] == '-') {
        path = "";
    }

    // relative or absolute path fix
    // TODO: Support for relative path syntax [..][~]
    if (path.length() && path[0] != '/') {
        // realative path asdf
        tp = cwd + "/" + path;
    } else {
        // absolute so use full path given
        if (path.length()) {
            tp = path;
        } else {
            tp = cwd;
        }
    }

    // fix empty path
    if (!tp.length()) {
        tp = "/";
    }
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "relative_path_fix ( cwd:'%s', path:'%s'->'%s', tp:'%s'->'%s')", cwd.c_str(), tpath.c_str(), path.c_str(), ttp.c_str(), tp.c_str());
#endif
}


/**
 * @brief Called at the start of a STOR request.
 *  The file name is the name of the file the client would like to save.
 */
void FTPDFileCallbacks::onStoreStart(std::string fileName)
{
    // build relative and actual file path
    std::string tp;
    relative_path_fix(ad2ftpd_cwd, fileName, tp);
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onStoreStart: %s)->'%s'", fileName.c_str(), tp.c_str());
#endif
    // FIXME: create any missing folders if needed or it will fail to copy folders.

    // Open the file for writing.
    m_storeFile.open(tp, std::ios::binary);
    if (m_storeFile.fail()) {
        ESP_LOGE(TAG, "Opening file: %s for write failed with error: %s",
                 tp.c_str(), strerror(errno));
        throw FTPD::FileException();
    }
}

/**
 * @brief Called when the client presents a new chunk of data to be saved.
 */
size_t FTPDFileCallbacks::onStoreData(uint8_t* data, size_t size)
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG,"onStoreData(data, size=%i)", size);
#endif
    // Store data received.
    m_storeFile.write((char *)data, size);
    return size;
}

/**
 * @brief Called at the end of a STOR request.
 * This indicates that the client has completed its transmission of the
 * file.
 */
void FTPDFileCallbacks::onStoreEnd()
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG,"onStoreEnd()");
#endif
    // Close the open file.
    m_storeFile.close();
}

/**
 * @brief Called when the client requests retrieval of a file.
 * @param fileName The name of the file name to retrieve.
 */
void FTPDFileCallbacks::onRetrieveStart(std::string fileName)
{
    // build relative and actual file path
    std::string tp;
    relative_path_fix(ad2ftpd_cwd, fileName, tp);
#if defined(FTPD_DEBUG)
    ESP_LOGE(TAG, "onRetrieveStart(%s)", tp.c_str());
#endif
    m_byteCount = 0;

    m_retrieveFile.open(tp, std::ios::binary);
    if (m_retrieveFile.fail()) {
        ESP_LOGE(TAG, "Opening file: %s for read failed with error: %s",
                 tp.c_str(), strerror(errno));
        throw FTPD::FileException();
    }
}

/**
 * @brief Called when the client is ready to receive the next piece of the file.  To indicate that there
 * is no more data to send, return a size of 0.
 * @param data The data buffer that we can fill to return data back to the client.
 * @param size The maximum size of the data buffer that we can populate.
 * @return The size of data being returned.  Return 0 to indicate that there is no more data to return.
 */
size_t FTPDFileCallbacks::onRetrieveData(uint8_t* data, size_t size)
{
    // read some data.
    m_retrieveFile.read((char *)data, size);
    size_t readSize = m_retrieveFile.gcount();
    m_byteCount += readSize;

    // Return the number of bytes read.
    return m_retrieveFile.gcount();
}

/**
 * @brief Called when the retrieval has been completed.
 */
void FTPDFileCallbacks::onRetrieveEnd()
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG,"onRetrieveEnd()");
#endif
    m_retrieveFile.close();
}


/**
 * @brief Return a list of files in a directory or a single file.
 * https://files.stairways.com/other/ftp-list-specs-info.txt
 * handle virtual folders by not using stat() just assume it is ok.
 * @return std::string formatted list of file(s).
 */
std::string FTPDFileCallbacks::onDir(std::string path)
{
    // build relative and actual file path
    std::string tp;
    relative_path_fix(ad2ftpd_cwd, path, tp);
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onDir: '%s' '%s'", tp.c_str(), path.c_str());
#endif
    // Can not stat(root) the mount point '/sdcard'!
    // so skip running state on root and the default
    // will be S_IFDIR
    struct stat file_stats;
    file_stats.st_mode = S_IFDIR;
    bool skip_stat = false;
    bool is_root = false;

    // Don't try and stat virtual spiffs folder or root folder.
    // FIXME: is the size check needed? Not if relative_path_fix forced one in tp
    if (!tp.size() || strcasecmp(tp.c_str(), "/") == 0) {
        is_root = true;
        skip_stat = true;
    }
    if (strcasecmp(tp.c_str(), "/" AD2_SPIFFS_MOUNT_POINT) == 0) {
        skip_stat = true;
    }

    // Test it file or directory exist.
    // note: No case compare for FAT fs.
    if (skip_stat || stat(tp.c_str(), &file_stats) == 0) {
#if defined(FTPD_DEBUG)
        ESP_LOGI(TAG, "stat: %i", file_stats.st_mode);
#endif
        // is it a folder? return directory list. skip root.
        if (S_ISDIR(file_stats.st_mode)) {
#if defined(FTPD_DEBUG)
            ESP_LOGI(TAG, "onDir: ISDIR %s", tp.c_str());
#endif
            std::stringstream ss;

            // Add virtual mount points AD2_SPIFFS_MOUNT_POINT and AD2_USD_MOUNT_POINT
            if (is_root) {
#if defined(FTPD_DEBUG)
                ESP_LOGI(TAG, "onDir: Insert special virtual entries.");
#endif
                file_stats.st_size = 0;
                file_stats.st_mode = S_IFDIR | S_IRWXU | S_IRWXO | S_IRWXG;
                ss << _format_dir_line(AD2_SPIFFS_MOUNT_POINT, &file_stats);
                ss << _format_dir_line(AD2_USD_MOUNT_POINT, &file_stats);
            } else
                // uSD or SPIFFS sub folders. SPIFFS is special does not
                // allow sub folders and has limits on file name etc.
                if (tp.find("/" AD2_USD_MOUNT_POINT) == 0 ||
                        tp.find("/" AD2_SPIFFS_MOUNT_POINT) == 0) {
                    DIR* dir = opendir(tp.c_str());
                    // should not happen but just in case.
                    if (!dir) {
                        ESP_LOGE(TAG,"opendir fail error: %s path: %s", strerror(errno), tp.c_str());
                        throw FTPD::FileException();
                    }
                    // read the directory and build return string.
                    while(1) {
                        struct dirent* pDirentry = readdir(dir);
                        if (pDirentry == nullptr) {
                            break;
                        }

                        // only files and folders nothing fancy.
                        if (pDirentry->d_type == DT_DIR || pDirentry->d_type == DT_REG) {

                            // get stat for actual file
                            std::string _tf = tp;
                            _tf += "/";
                            _tf += pDirentry->d_name;
                            stat(_tf.c_str(), &file_stats);
#if defined(FTPD_DEBUG)
                            ESP_LOGI(TAG, "st_mode: %08x '%s'", file_stats.st_mode, _tf.c_str());
#endif
                            // format each file with the 'path' given as the base path.
                            ss << _format_dir_line(pDirentry->d_name, &file_stats);
                        }
                    }
                    closedir(dir);
                }

            return ss.str();
        } else if (S_ISREG(file_stats.st_mode)) {
#if defined(FTPD_DEBUG)
            ESP_LOGI(TAG, "onDir: ISREG");
#endif
            // just want the file not full path.
            path = path.substr(path.find_last_of("/") + 1);
            return _format_dir_line(path.c_str(), &file_stats);
        } else {
            // unknown type. skip.
#if defined(FTPD_DEBUG)
            ESP_LOGI(TAG,"Unknown type %08x", file_stats.st_mode);
#endif
            throw FTPD::FileException();
        }

    } else {
#if defined(FTPD_DEBUG)
        ESP_LOGI(TAG,"stat failed?  error: %s", strerror(errno));
#endif
        throw FTPD::FileException();
    }
}

// FTPDCallbacks template functions
void FTPDCallbacks::onStoreStart(std::string fileName) {}
size_t FTPDCallbacks::onStoreData(uint8_t* data, size_t size)
{
    return 0;
}
void FTPDCallbacks::onStoreEnd() {}
void FTPDCallbacks::onRetrieveStart(std::string fileName) {}
size_t FTPDCallbacks::onRetrieveData(uint8_t *data, size_t size)
{
    return 0;
}
void FTPDCallbacks::onRetrieveEnd() {}
std::string FTPDCallbacks::onDir(std::string path)
{
    return "";
}
FTPDCallbacks::~FTPDCallbacks() {}
// End FTPDCallbacks template functions

FTPD::FTPD()
{
    m_dataSocket    = -1;
    m_clientSocket  = -1;
    m_dataPort      = -1;
    m_dataIp        = -1;
    m_passiveSocket = -1;
    m_serverSocket  = -1;

    m_callbacks       = nullptr;
    m_isPassive       = false;
    m_isImage         = true;
    m_chunkSize       = 2048;
    m_port            = 21;
    m_loginRequired   = false;
    m_isAuthenticated = false;
    m_userid = "";
    m_password = "";
    m_save = "";
    m_save_clear = 0;
}

// @brief cleanup if any
FTPD::~FTPD()
{
}

/**
 * @brief Close the connection to the FTP client.
 */
void FTPD::closeConnection()
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "closeConnection fd: %i", m_clientSocket);
#endif
    // close passive connection if any also
    closePassive();

    if (m_clientSocket != -1) {
        close(m_clientSocket);
        m_clientSocket = -1;
    }
    // clear out last connection state
    m_save_clear = 0;
    m_save = "";
    m_isAuthenticated = false;
    m_lastCommand = "";
}

/**
 * @brief Close a previously opened data connection.
 */
void FTPD::closeData()
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "closeData() fd: %i", m_dataSocket);
#endif
    if (m_dataSocket != -1) {
        close(m_dataSocket);
        m_dataSocket = -1;
    }
}

/**
 * @brief Close the passive listening socket that was opened by listenPassive.
 */
void FTPD::closePassive()
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "closePassive() fd: %i", m_passiveSocket);
#endif
    if (m_passiveSocket != -1) {
        close(m_passiveSocket);
        m_passiveSocket = -1;
    }
}

/**
 * @brief Create a listening socket for the new passive connection.
 * @return a String for the passive parameters.
 * FIXME: IPv6
 */
std::string FTPD::listenPassive()
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "listenPassive()");
#endif
    int rc;
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(0);
    struct sockaddr_in clientAddrInfo;

    // don't create again if already done.
    if (m_passiveSocket == -1) {
        m_passiveSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_passiveSocket == -1) {
            ESP_LOGE(TAG, "m_passiveSocket = socket() error: %s", strerror(errno));
            return "ERROR";
        }

        rc = bind(m_passiveSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
        if (rc == -1) {
            ESP_LOGE(TAG, "m_passiveSocket bind() error %s", strerror(errno));
            closePassive();
            return "ERROR";
        }

        rc = listen(m_passiveSocket, 5);
        if (rc == -1) {
            ESP_LOGE(TAG, "m_passiveSocket listen() error %s", strerror(errno));
            closePassive();
            return "ERROR";
        }
    }

#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "m_passiveSocket fd: %i", m_passiveSocket);
#endif

    unsigned int addrLen = sizeof(serverAddress);
    rc = getsockname(m_passiveSocket, (struct sockaddr*)&serverAddress, &addrLen);
    if (rc == -1) {
        ESP_LOGE(TAG, "m_passiveSocket getsockname() error %s", strerror(errno));
        closePassive();
        return "ERROR";
    }

    unsigned int addrInfoSize = sizeof(clientAddrInfo);
    getsockname(m_clientSocket, (struct sockaddr*)&clientAddrInfo, &addrInfoSize);

    // Convert client address to string for ACL testing.
    // std::string IP;
    // hal_get_socket_client_ip(m_passiveSocket, IP);

    std::stringstream ss;
    ss    << ((clientAddrInfo.sin_addr.s_addr >> 0)  & 0xff) <<
          "," << ((clientAddrInfo.sin_addr.s_addr >> 8)  & 0xff) <<
          "," << ((clientAddrInfo.sin_addr.s_addr >> 16) & 0xff) <<
          "," << ((clientAddrInfo.sin_addr.s_addr >> 24) & 0xff) <<
          "," << ((serverAddress.sin_port >> 0) & 0xff) <<
          "," << ((serverAddress.sin_port >> 8) & 0xff);
    std::string retStr = ss.str();

    return retStr;
}

/**
 * @brief Handle the AUTH command.
 */
void FTPD::onAuth(std::istringstream& ss)
{
    std::string param;
    ss >> param;
    sendResponse(FTPD::RESPONSE_500_COMMAND_UNRECOGNIZED);                // Syntax error, command unrecognized.
}

/**
 * @brief Change the current working directory.
 * @param ss A string stream where the first parameter is the directory to change to.
 * Possible responses:
 * 250
 * 421,
 * 500, 501, 502, 530, 550
 */
void FTPD::onCwd(std::istringstream& ss)
{
    // get all remaining stream as path
    std::string path;
    getline(ss, path, '\r');

    // build relative and actual file path
    std::string tp;
    relative_path_fix(ad2ftpd_cwd, path, tp);
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onCwd('%s') '%s'", path.c_str(), tp.c_str());
#endif
    // skip stat on virtual spiffs folder or root folder.
    bool skip_stat = false;
    if (tp.size() || strcasecmp(tp.c_str(), "/") == 0) {
        skip_stat = true;
    } else if (strcasecmp(tp.c_str(), "/" AD2_SPIFFS_MOUNT_POINT) == 0) {
        skip_stat = true;
    }

    // Test it file or directory exist.
    // note: No case compare for FAT fs.
    if (skip_stat) {
        ad2ftpd_cwd = tp;
        sendResponse(FTPD::RESPONSE_200_COMMAND_OK);
    } else {
        // test and accept if valid folder
        struct stat s;
        int err = stat(tp.c_str(), &s);
        if(err == -1) {
            if(ENOENT == errno) {
                /* does not exist */
                sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
            } else {
                sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
            }
        } else {
            if(S_ISDIR(s.st_mode)) {
                /* it's a dir save it */
                ad2ftpd_cwd = tp;
                sendResponse(FTPD::RESPONSE_200_COMMAND_OK);
            } else {
                /* exists but is no dir */
                sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
            }
        }
    }
}

/**
 * @brief Change the current working directory up to parent.
 * @note Possible responses:
 *   421
 *   500, 501, 502, 530, 550
 */
void FTPD::onCdup(std::istringstream& ss)
{
    // Rebuild cwd dropping last folder.
    std::string path;
    std::vector<std::string> parts;
    ad2_tokenize(ad2ftpd_cwd, "/", parts);
    if (parts.size()) {
        parts.pop_back();
        for (auto& it : parts) {
            path += "/";
            path += it;
        }
    }
#if defined(FTPD_DEBUG)
    ESP_LOGE(TAG, "onCdup: '%s'", path.c_str());
#endif
    std::string dirString;
    try {
        std::string dirString = m_callbacks->onDir(path);
        // save path if ok
        ad2ftpd_cwd = path;
        std::string temp = "\"" + ad2ftpd_cwd + "\"";
        sendResponse(257, temp);
    } catch(FTPD::FileException& e) {
        // Requested action not taken.
        sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
        return;
    }

}

/**
 * @brief Process the client transmitted LIST request.
 */
void FTPD::onList(std::istringstream& ss)
{
    // get all remaining stream as directory
    std::string directory;
    getline(ss, directory, '\r');
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onList('%s')", directory.c_str());
#endif
    openData();
    if (m_callbacks != nullptr) {
        std::string dirString;
        try {
            dirString = m_callbacks->onDir(directory);
        } catch(FTPD::FileException& e) {
            // Requested action not taken.
            sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
            closeData();
            return;
        }
        // File status okay; about to open data connection.
        sendResponse(FTPD::RESPONSE_150_ABOUT_TO_OPEN_DATA_CONNECTION);
        sendData((uint8_t *)dirString.data(), dirString.length());
    }
    closeData();

    sendResponse(FTPD::RESPONSE_226_CLOSING_DATA_CONNECTION); // Closing data connection.
}

/**
 * @brief Process the client transmitted DELE request.
 */
void FTPD::onDele(std::istringstream &ss)
{
    // get all remaining stream as path
    std::string path;
    getline(ss, path, '\r');

    // build relative and actual file path
    std::string tp;
    relative_path_fix(ad2ftpd_cwd, path, tp);
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onDele('%s') '%s'", path.c_str(), tp.c_str());
#endif
    // delete the file return results
    if (::unlink(tp.c_str()) != 0) {
        sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
        return;
    }
    sendResponse(FTPD::RESPONSE_200_COMMAND_OK);
}

/**
 * @brief Process the client transmitted RNFR request.
 */
void FTPD::onRnfr(std::istringstream &ss)
{
    // get all remaining stream as path
    std::string path;
    getline(ss, path, '\r');

    // build relative and actual file path
    std::string tp;
    relative_path_fix(ad2ftpd_cwd, path, tp);

    // Test if file is valid respond with OK/FAIL
    struct stat s;
    int err = stat(tp.c_str(), &s);
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onRnfr('%s') '%s'", path.c_str(), tp.c_str());
#endif
    if (!S_ISDIR(s.st_mode) && !S_ISREG(s.st_mode)) {
        sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
        return;
    }
    // FIXME: Best to clear this if next command is not RNTO.
    m_save = tp;
    m_save_clear = 1;
    sendResponse(FTPD::RESPONSE_350_RESPONSE_CODE);
}

/**
 * @brief Process the client transmitted RNTO request.
 */
void FTPD::onRnto(std::istringstream &ss)
{
    // get all remaining stream as path
    std::string path;
    getline(ss, path, '\r');

    // build relative and actual file path
    std::string tp;
    relative_path_fix(ad2ftpd_cwd, path, tp);
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onRnto('%s') '%s' m_save: '%s'", path.c_str(), tp.c_str(), m_save.c_str());
#endif
    // Confirm and attempt to rename return results.
    if (!m_save.length()) {
        sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
        return;
    }
    // make the folder return results
    if (::rename(m_save.c_str(), tp.c_str()) != 0) {
        sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
        return;
    }
    sendResponse(FTPD::RESPONSE_200_COMMAND_OK);
}

/**
 * @brief Process the client transmitted RMD request.
 */
void FTPD::onRmd(std::istringstream &ss)
{
    // get all remaining stream as path
    std::string path;
    getline(ss, path, '\r');

    // build relative and actual file path
    std::string tp;
    relative_path_fix(ad2ftpd_cwd, path, tp);
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onRmd('%s') '%s'", path.c_str(), tp.c_str());
#endif
    // remove the folder return results
    if (::rmdir(tp.c_str()) != 0) {
        sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
        return;
    }
    sendResponse(FTPD::RESPONSE_200_COMMAND_OK);
}


/**
 * @brief Process the client transmitted MKD request.
 */
void FTPD::onMkd(std::istringstream &ss)
{
    // get all remaining stream as path
    std::string path;
    getline(ss, path, '\r');

    // build relative and actual file path
    std::string tp;
    relative_path_fix(ad2ftpd_cwd, path, tp);
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onMkd('%s') '%s'", path.c_str(), tp.c_str());
#endif
    // make the folder return results
    if (::mkdir(tp.c_str(), 0) != 0) {
        sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
        return;
    }
    sendResponse(FTPD::RESPONSE_200_COMMAND_OK);
}


/**
 * @brief Process a REST operation.
 * Restart the AD2IoT
 */
void FTPD::onRest(std::istringstream& ss)
{
    sendResponse(RESPONSE_200_COMMAND_OK, "Rebooting ad2iot now."); // Command okay.
    // Warning: This will bypass check to save config if changed in memory.
    // The intent is to upload a new ad2iot.ini and 'REST' the device to
    // load this new config abandoning any running configuration that my exist.
    ad2_printf_host(true, "%s: 'REST' command received. Restarting system now.", TAG);
    closeConnection();
    hal_restart();
}

/**
 * @brief Process a NOOP operation.
 */
void FTPD::onNoop(std::istringstream& ss)
{
    sendResponse(RESPONSE_200_COMMAND_OK); // Command okay.
}

/**
 * @brief Process PORT request.  The information provided is encoded in the parameter as
 * h1,h2,h3,h4,p1,p2 where h1,h2,h3,h4 is the IP address we should connect to
 * and p1,p2 is the port number.  The data is MSB
 *
 * Our logic does not form any connection but remembers the ip address and port number
 * to be used for a subsequence data connection.
 *
 * FIXME: IPv6
 *
 * Possible responses:
 * 200
 * 500, 501, 421, 530
 */
void FTPD::onPort(std::istringstream& ss)
{
    char c;
    uint16_t h1, h2, h3, h4, p1, p2;
    ss >> h1 >> c >> h2 >> c >> h3 >> c >> h4 >> c >> p1 >> c >> p2;
    m_dataPort = p1*256 + p2;
    m_dataIp = h1<<24 | h2<<16 | h3<<8 | h4;
#if defined(FTPD_DEBUG)
    ESP_LOGE(TAG, "onPort Ip:%i.%i.%i.%i Port:%i", h1, h2, h3, h4, m_dataPort);
#endif
    sendResponse(RESPONSE_200_COMMAND_OK); // Command okay.
    m_isPassive = false;
}

/**
 * @brief Process the PASS command.
 * Possible responses:
 * 230
 * 202
 * 530
 * 500, 501, 503, 421
 * 332
 */
void FTPD::onPass(std::istringstream& ss)
{
    // get all remaining stream as password
    std::string password;
    getline(ss, password, '\r');

    // If the immediate last command wasn't USER then don't try and process PASS.
    if (m_lastCommand != "USER") {
        sendResponse(RESPONSE_503_BAD_SEQUENCE);
        printf("<< onPass\n");
        return;
    }

    // Compare the supplied userid and passwords.
    if (m_userid == m_suppliedUserid && password == m_password) {
        sendResponse(RESPONSE_230_USER_LOGGED_IN);
        m_isAuthenticated = true;
    } else {
        sendResponse(RESPONSE_530_NOT_LOGGED_IN);
        closeConnection();
        m_isAuthenticated = false;
    }
}

/**
 * @brief Process the PASV command.
 * Possible responses:
 * 227
 * 500, 501, 502, 421, 530
 */
void FTPD::onPasv(std::istringstream& ss)
{
    std::string ipInfo = listenPassive();
    std::ostringstream responseTextSS;
    responseTextSS << "Entering Passive Mode (" << ipInfo << ").";
    std::string responseText;
    responseText = responseTextSS.str();
    sendResponse(RESPONSE_227_ENTERING_PASSIVE_MODE, responseText.c_str());
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onPasv: => '%s'", responseText.c_str());
#endif
    m_isPassive = true;
}

/**
 * @brief Process the PWD command to determine our current working directory.
 * Possible responses:
 * 257
 * 500, 501, 502, 421, 550
 */
void FTPD::onPWD(std::istringstream& ss)
{
    // build relative and actual file path
    std::string tp;
    std::string path;
    relative_path_fix(ad2ftpd_cwd, path, tp);
    std::string temp = "\"" + tp + "\"";
    sendResponse(257, temp);
#if defined(FTPD_DEBUG)
    ESP_LOGE(TAG, "onPWD: '%s'", temp.c_str());
#endif
}

/**
 * @brief Possible responses:
 * 221
 * 500
 */
void FTPD::onQuit(std::istringstream& ss)
{
    sendResponse(FTPD::RESPONSE_221_CLOSING_CONTROL_CONNECTION); // Service closing control connection.
    closeConnection();  // Close the connection to the client.
}

/**
 * @brief Process a RETR command.  The client sends this command to retrieve the content of a file.
 * The name of the file is the first parameter in the input stream.
 *
 * Possible responses:
 * 125, 150
 *   (110)
 *   226, 250
 *   425, 426, 451
 * 450, 550
 * 500, 501, 421, 530
 * @param ss The parameter stream.
 */
void FTPD::onRetr(std::istringstream& ss)
{
    // We open a data connection back to the client.  We then invoke the callback to indicate that we have
    // started a retrieve operation.  We call the retrieve callback to request the next chunk of data and
    // transmit this down the data connection.  We repeat this until there is no more data to send at which
    // point we close the data connection and we are done.

    // get all remaining stream as fileName
    std::string fileName;
    getline(ss, fileName, '\r');

    uint8_t data[m_chunkSize];
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onRetr <= '%s'", fileName.c_str());
#endif
    // close just in case? ok sure.
    if (m_callbacks != nullptr) {
        m_callbacks->onRetrieveEnd();
    }

    if (m_callbacks != nullptr) {
        try {
            m_callbacks->onRetrieveStart(fileName);
        } catch(FTPD::FileException& e) {
            // Requested action not taken.
            sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN);
            return;
        }
    }
    // File status okay; about to open data connection.
    sendResponse(FTPD::RESPONSE_150_ABOUT_TO_OPEN_DATA_CONNECTION);

    openData();
    if (m_callbacks != nullptr) {
        int readSize = m_callbacks->onRetrieveData(data, m_chunkSize);
        while(readSize > 0) {
            sendData(data, readSize);
            readSize = m_callbacks->onRetrieveData(data, m_chunkSize);
        }
    }
    closeData();

    // Closing data connection.
    sendResponse(FTPD::RESPONSE_226_CLOSING_DATA_CONNECTION);
    if (m_callbacks != nullptr) {
        m_callbacks->onRetrieveEnd();
    }
}

/**
 * @brief Called to process a STOR request.  This means that the client wishes to store a file
 * on the server.  The name of the file is found in the parameter.
 * @param ss The parameter stream.
 */
void FTPD::onStor(std::istringstream& ss)
{
    // get all remaining stream as fileName
    std::string fileName;
    getline(ss, fileName, '\r');
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onStor <= '%s'", fileName.c_str());
#endif
    receiveFile(fileName);
}

/**
 * @brief Called to process Syst request.
 * @param ss The parameter stream.
 */
void FTPD::onSyst(std::istringstream& ss)
{
    // get all remaining stream as sys
    std::string sys;
    getline(ss, sys, '\r');
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onSyst <= '%s'", sys.c_str());
#endif
    sendResponse(215, "UNIX Type: L8");
}

/**
 * @brief Process a TYPE request.  The parameter that follows is the type of transfer we wish
 * to process.  Types include:
 * I and A.
 *
 * Possible responses:
 * 200
 * 500, 501, 504, 421, 530
 * @param ss The parameter stream.
 */
void FTPD::onType(std::istringstream& ss)
{
    std::string type;
    ss >> type;
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onType <= '%s'", type.c_str());
#endif
    if (type.compare("I") == 0) {
        m_isImage = true;
    } else {
        m_isImage = false;
    }
    sendResponse(FTPD::RESPONSE_200_COMMAND_OK);   // Command okay.
}

/**
 * @brief Process a USER request.  The parameter that follows is the identity of the user.
 *
 * Possible responses:
 * 230
 * 530
 * 500, 501, 421
 * 331, 332
 * @param ss The parameter stream.
 */
void FTPD::onUser(std::istringstream& ss)
{
    // When we receive a user command, we next want to know if we should ask for a password.  If the m_loginRequired
    // flag is set then we do indeed want a password and will send the response that we wish one.

    // get all remaining stream as userName
    std::string userName;
    getline(ss, userName, '\r');
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onUser <= '%s'", userName.c_str());
#endif
    if (m_loginRequired) {
        sendResponse(FTPD::RESPONSE_331_PASSWORD_REQUIRED);
    } else {
        sendResponse(FTPD::RESPONSE_200_COMMAND_OK); // Command okay.
    }
    m_suppliedUserid = userName;   // Save the username that was supplied.
}

/**
 * @brief Process a XMKD request.
 * @param ss The parameter stream.
 */
void FTPD::onXmkd(std::istringstream &ss)
{
    std::string tmp;
    ss >> tmp;
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onXmkd '%s'", tmp.c_str());
#endif
    sendResponse(FTPD::RESPONSE_500_COMMAND_UNRECOGNIZED);
}

/**
 * @brief Process a XRMD request.
 * @param ss The parameter stream.
 */
void FTPD::onXrmd(std::istringstream &ss)
{
    std::string tmp;
    ss >> tmp;
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "onXrmd <= '%s'", tmp.c_str());
#endif
    sendResponse(FTPD::RESPONSE_500_COMMAND_UNRECOGNIZED);
}

/**
 * @brief Open a data connection with the client.
 * We will use closeData() to close the connection.
 * @return True if the data connection succeeded.
 */
bool FTPD::openData()
{
    if (m_isPassive) {
        // Handle a passive connection ... here we receive a connection from the client from the passive socket.
        struct sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);
        m_dataSocket = accept(m_passiveSocket, (struct sockaddr *)&clientAddress, &clientAddressLength);
        if (m_dataSocket == -1) {
#if defined(FTPD_DEBUG)
            ESP_LOGE(TAG, "FTPD::openData: accept() error: %s", strerror(errno));
#endif
            closePassive();
            return false;
        }
        closePassive();
    } else {
        // Handle an active connection ... here we connect to the client.
        m_dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in serverAddress;
        serverAddress.sin_family      = AF_INET;
        serverAddress.sin_addr.s_addr = htonl(m_dataIp);
        serverAddress.sin_port        = htons(m_dataPort);

        int rc = connect(m_dataSocket, (struct sockaddr *)&serverAddress, sizeof(struct sockaddr_in));
        if (rc == -1) {
#if defined(FTPD_DEBUG)
            ESP_LOGE(TAG, "FTPD::openData: connect(): %s", strerror(errno));
#endif
            return false;
        }
    }
    return true;
}

/**
 * @brief Process commands received from the client.
 */
void FTPD::processCommand()
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "ProcessCommand start.");
#endif
    sendResponse(FTPD::RESPONSE_220_SERVICE_READY); // Service ready.
    m_lastCommand = "";
    while(1) {
        std::string line = "";
        char currentChar;
        char lastChar = '\0';
        int rc = recv(m_clientSocket, &currentChar, 1, 0);
        while(rc != -1 && rc!=0) {
            line += currentChar;
            if (lastChar == '\r' && currentChar == '\n') {
                break;
            }
            lastChar = currentChar;
            rc = recv(m_clientSocket, &currentChar, 1, 0);
        } // End while we are waiting for a line.

        if (rc == 0 || rc == -1) {          // If we didn't get a line or an error, then we have finished processing commands.
            break;
        }

        std::string command;
        std::istringstream ss(line);
        getline(ss, command, ' ');
        ad2_trim(command);

        // clear saved data if expired.
        if (!m_save_clear--) {
            m_save = "";
        }

        // We now have a command to process.

        if (command.compare("USER")==0) {
            onUser(ss);
        } else if (command.compare("PASS")==0) {
            onPass(ss);
        } else if (m_loginRequired && !m_isAuthenticated) {
            sendResponse(RESPONSE_530_NOT_LOGGED_IN);
        } else if (command.compare("PASV")==0) {
            onPasv(ss);
        } else if (command.compare("SYST")==0) {
            onSyst(ss);
        } else if (command.compare("PORT")==0) {
            onPort(ss);
        } else if (command.compare("LIST")==0) {
            onList(ss);
        } else if (command.compare("TYPE")==0) {
            onType(ss);
        } else if (command.compare("RETR")==0) {
            onRetr(ss);
        } else if (command.compare("QUIT")==0) {
            onQuit(ss);
        } else if (command.compare("AUTH")==0) {
            onAuth(ss);
        } else if (command.compare("STOR")==0) {
            onStor(ss);
        } else if (command.compare("DELE")==0) {
            onDele(ss);
        } else if (command.compare("PWD")==0) {
            onPWD(ss);
        } else if (command.compare("MKD")==0) {
            onMkd(ss);
        } else if (command.compare("XMKD")==0) {
            onXmkd(ss);
        } else if (command.compare("RMD")==0) {
            onRmd(ss);
        } else if (command.compare("XRMD")==0) {
            onXrmd(ss);
        } else if (command.compare("CWD")==0) {
            onCwd(ss);
        } else if (command.compare("CDUP")==0) {
            onCdup(ss);
        } else if (command.compare("RNFR")==0) {
            onRnfr(ss);
        } else if (command.compare("RNTO")==0) {
            onRnto(ss);
        } else if (command.compare("REST")==0) { // Custom verb to restart AD2IoT
            onRest(ss);
        } else {
#if defined(FTPD_DEBUG)
            ESP_LOGI(TAG, "Unknown command verb: '%s'. Teach me more please!", command.c_str());
#endif
            sendResponse(FTPD::RESPONSE_500_COMMAND_UNRECOGNIZED); // Syntax error, command unrecognized.
        }
        m_lastCommand = command;
    } // End loop processing commands.

#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "ProcessCommand done. Closing any open connections.");
#endif
    closeConnection();
}

/**
 * @brief Receive a file from the FTP client (STOR).
 * The name of the file to be created is passed as a parameter.
 * @param fileName filename for operation.
 */
void FTPD::receiveFile(std::string fileName)
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "receiveFile(%s)", fileName.c_str());
#endif
    if (m_callbacks != nullptr) {
        try {
            m_callbacks->onStoreStart(fileName);
        } catch(FTPD::FileException& e) {
            ESP_LOGE(TAG, "onStoreStart exception %s", strerror(errno));
            sendResponse(FTPD::RESPONSE_550_ACTION_NOT_TAKEN); // Requested action not taken.
            return;
        }
    }

    openData();
    sendResponse(FTPD::RESPONSE_150_ABOUT_TO_OPEN_DATA_CONNECTION); // File status okay; about to open data connection.
    uint8_t buf[m_chunkSize];
    uint32_t totalSizeRead = 0;
    while(1) {
        int rc = recv(m_dataSocket, &buf, m_chunkSize, 0);
        if (rc <= 0) {
            break;
        }
        if (m_callbacks != nullptr) {
#if defined(FTPD_DEBUG)
            ESP_LOGI(TAG, "onStoreData(size: %i)", rc);
#endif
            int storeSize = m_callbacks->onStoreData(buf, rc);
            if (!storeSize) {
                ESP_LOGE(TAG, "onStoreData(size: %i) failed", rc);
                break;
            }
        }
        totalSizeRead += rc;
    }
    sendResponse(FTPD::RESPONSE_226_CLOSING_DATA_CONNECTION); // Closing data connection.
    closeData();

    if (m_callbacks != nullptr) {
        m_callbacks->onStoreEnd();
    }
}

/**
 * @brief Send data to the client over the data connection
 * previously opened with a call to openData().
 * @param pData A pointer to the data to send.
 * @param size The number of bytes to send.
 */
void FTPD::sendData(uint8_t* pData, uint32_t size)
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "FTPD::sendData: send(): size: %i", size);
#endif
    int rc = send(m_dataSocket, pData, size, 0);
    if (rc == -1) {
        // probably disconnected.
        ESP_LOGW(TAG, "FTPD::sendData: send(): %s", strerror(errno));
    }
}

/**
 * @brief Send a response to the client.  A response is composed
 * of two parts.  The first is a code as architected in the
 * FTP specification.  The second is a piece of text.
 * @param code the response code to send.
 * @param text the responst text to send.
 */
void FTPD::sendResponse(int code, std::string text)
{
    std::ostringstream ss;
    ss << code << " " << text << "\r\n";
    int rc = send(m_clientSocket, ss.str().data(), ss.str().length(), 0);
}

/**
 * @brief Send a response to the client.  A response is composed of two parts.
 * The first is a code as architected in the FTP specification.  The second is
 * a piece of text.  In this function, a standard piece of text is used based on
 * the code.
 * @param code the response code to send.
 */
void FTPD::sendResponse(int code)
{
    std::string text = "unknown";

    switch(code) {             // Map the code to a text string.
    case RESPONSE_150_ABOUT_TO_OPEN_DATA_CONNECTION:
        text = "File status okay; about to open data connection.";
        break;
    case RESPONSE_200_COMMAND_OK:
        text = "Command okay.";
        break;
    case RESPONSE_220_SERVICE_READY:
        text = "Service ready.";
        break;
    case RESPONSE_221_CLOSING_CONTROL_CONNECTION:
        text = "Service closing control connection.";
        break;
    case RESPONSE_226_CLOSING_DATA_CONNECTION:
        text = "Closing data connection.";
        break;
    case RESPONSE_230_USER_LOGGED_IN:
        text = "User logged in, proceed.";
        break;
    case RESPONSE_331_PASSWORD_REQUIRED:
        text = "Password required.";
        break;
    case RESPONSE_500_COMMAND_UNRECOGNIZED:
        text = "Syntax error, command unrecognized.";
        break;
    case RESPONSE_502_COMMAND_NOT_IMPLEMENTED:
        text = "Command not implemented.";
        break;
    case RESPONSE_503_BAD_SEQUENCE:
        text = "Bad sequence of commands.";
        break;
    case RESPONSE_530_NOT_LOGGED_IN:
        text = "Not logged in.";
        break;
    case RESPONSE_550_ACTION_NOT_TAKEN:
        text = "Requested action not taken.";
        break;
    default:
        break;
    }
    sendResponse(code, text);   // Send the code AND the text to the FTP client.
}

/**
 * @brief Set the callbacks that are to be invoked to perform work.
 * @param pCallbacks An instance of an FTPDCallbacks based class.
 */
void FTPD::setCallbacks(FTPDCallbacks* pCallbacks)
{
    m_callbacks = pCallbacks;
}

/**
 * @brief Set the credentials allowed to connect.
 * @param userid user id allowed to connect.
 * @param password for id.
 */
void FTPD::setCredentials(std::string userid, std::string password)
{
    m_loginRequired = true;
    m_userid        = userid;
    m_password      = password;
}

/**
 * @brief Set the TCP port we should listen on for FTP client requests.
 * @param port number to listen on.
 */
void FTPD::setPort(uint16_t port)
{
    m_port = port;
}


/**
 * @brief Start being an FTP Server.
 */
void FTPD::start()
{
#if CONFIG_LWIP_IPV6
    int addr_family = AF_INET6;
    struct sockaddr_in6 dest_addr;
#else
    int addr_family = AF_INET;
    struct sockaddr_in dest_addr;
#endif

    m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_serverSocket == -1) {
        ESP_LOGE(TAG, "Failed to create listen socket. Exiting with error: %s", strerror(errno));
        return;
    }
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "crated server socket() fd: %i", m_serverSocket);
#endif

    // set REUSE TRUE
    int enable = 1;
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(m_port);
    int rc = bind(m_serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (rc == -1) {
        ESP_LOGE(TAG, "Server socket bind() failed. Exiting with error: %s", strerror(errno));
        return;
    }
    rc = listen(m_serverSocket, 5);
    if (rc == -1) {
        ESP_LOGE(TAG, "Server socket listen() failed. Exiting with error: %s", strerror(errno));
        return;
    }

    // NOTE: Only one connection allowed.
    while(1) {
        waitForFTPClient();
        processCommand();
    }
}

/**
 * @brief Wait for a new client to connect.
 * @return socket
 */
int FTPD::waitForFTPClient()
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "waitForFTPClient() start.");
#endif
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    m_clientSocket = accept(m_serverSocket, (struct sockaddr *)&clientAddress, &clientAddressLength);

#if defined(FTPD_DEBUG)
    ESP_LOGE(TAG, "waitForFTPClient accept finished %i", m_clientSocket);
#endif

    // first test for a valid socket
    if (m_clientSocket != -1) {

        // Convert client address to string for ACL testing.
        std::string IP;
        hal_get_socket_client_ip(m_clientSocket, IP);

        /* preform ACL test */
        if (!ad2ftpd_acl.find(IP)) {
            ESP_LOGI(TAG, "ACL reject connect from %s", IP.c_str());
            closeConnection();
            return -1;
        }
#if defined(FTPD_DEBUG)
        ESP_LOGI(TAG, "waitForFTPClient() finish.");
#endif
        // reset the path to the virtual root for each new connection.
        ad2ftpd_cwd = "";

        return m_clientSocket;
    } else {
        ESP_LOGE(TAG, "m_serverSocket accept() failed %s", strerror(errno));
        return -1;
    }
}

/**
 * @brief ftpd generic command event processing
 *  command: [COMMAND] <id> <arg>
 * ex.
 *   [COMMAND] 0 arg...
 */
static void _cli_cmd_ftpd_event(const char *string)
{

    // key value validation
    std::string cmd;
    ad2_copy_nth_arg(cmd, string, 0);
    ad2_lcase(cmd);

    if(cmd.compare(FTPD_COMMAND) != 0) {
        ad2_printf_host(false, "What?\r\n");
        return;;
    }

    // key value validation
    std::string subcmd;
    ad2_copy_nth_arg(subcmd, string, 1);
    ad2_lcase(subcmd);

    int i;
    bool en;
    for(i = 0;; ++i) {
        if (FTPD_SUBCMD[i] == 0) {
            ad2_printf_host(false, "What?\r\n");
            break;
        }
        if(subcmd.compare(FTPD_SUBCMD[i]) == 0) {
            std::string arg;
            std::string acl;
            switch(i) {
            /**
             * Enable/Disable ftp daemon.
             */
            case FTPD_SUBCMD_ENABLE_ID:
                if (ad2_copy_nth_arg(arg, string, 2) >= 0) {
                    ad2_set_config_key_bool(FTPD_CONFIG_SECTION, FTPD_SUBCMD_ENABLE, (arg[0] == 'Y' || arg[0] ==  'y'));
                    ad2_printf_host(false, "Success setting value. Restart required to take effect.\r\n");
                }

                // show contents of this slot
                en = false;
                ad2_get_config_key_bool(FTPD_CONFIG_SECTION, FTPD_SUBCMD_ENABLE, &en);
                ad2_printf_host(false, "ftp daemon is '%s'.\r\n", (en ? "Enabled" : "Disabled"));
                break;
            /**
             * ftp daemon IP/CIDR ACL list.
             */
            case FTPD_SUBCMD_ACL_ID:
                // If no arg then return ACL list
                if (ad2_copy_nth_arg(arg, string, 2, true) >= 0) {
                    ad2ftpd_acl.clear();
                    int res = ad2ftpd_acl.add(arg);
                    if (res == ad2ftpd_acl.ACL_FORMAT_OK) {
                        ad2_set_config_key_string(FTPD_CONFIG_SECTION, FTPD_SUBCMD_ACL, arg.c_str());
                    } else {
                        ad2_printf_host(false, "Error parsing ACL string. Check ACL format. Not saved.\r\n");
                    }
                }
                // show contents of this slot set default to allow all
                acl = "0.0.0.0/0";
                ad2_get_config_key_string(FTPD_CONFIG_SECTION, FTPD_SUBCMD_ACL, acl);
                ad2_printf_host(false, "ftpd 'acl' set to '%s'.\r\n", acl.c_str());
                break;
            default:
                break;
            }
            break;
        }
    }
}

/**
 * @brief ftp daemon task
 *
 * @param [in]pvParameters currently not used NULL.
 */
void ftp_daemon_task(void *pvParameters)
{
#if defined(FTPD_DEBUG)
    ESP_LOGI(TAG, "ftp daemon task starting.");
#endif
    if (ad2ftpd != nullptr) {
        ad2ftpd->start();
    }
    vTaskDelete(NULL);
}


/**
 * @brief command list for module
 */
static struct cli_command ftpd_cmd_list[] = {
    {
        (char*)FTPD_COMMAND,(char*)
        "####  Configuration for ftp server\r\n"
        "- ```" FTPD_COMMAND " {sub command} {arg}```\r\n"
        "  - {sub command}\r\n"
        "    - [" FTPD_SUBCMD_ENABLE "] Enable / Disable ftp daemon\r\n"
        "      -  {arg1}: [Y]es [N]o\r\n"
        "        - [N] Default state\r\n"
        "        - Example: ```" FTPD_COMMAND " " FTPD_SUBCMD_ENABLE " Y```\r\n"
        "    - [" FTPD_SUBCMD_ACL "] Set / Get ACL list\r\n"
        "      - {arg1}: ACL LIST\r\n"
        "      -  String of CIDR values separated by commas.\r\n"
        "        - Default: Empty string disables ACL list\r\n"
        "        - Example: ```" FTPD_COMMAND " " FTPD_SUBCMD_ACL " 192.168.0.0/28,192.168.1.0-192.168.1.10,192.168.3.4```\r\n\r\n", _cli_cmd_ftpd_event
    }
};

/**
 * @brief Register componet cli commands.
 */
void ftpd_register_cmds()
{
    // Register ftpd CLI commands
    for (int i = 0; i < ARRAY_SIZE(ftpd_cmd_list); i++) {
        cli_register_command(&ftpd_cmd_list[i]);
    }
}

/**
 * @brief AD2IoT Component ftpd init
 */
void ftpd_init()
{
    // if netif not enabled then we can't start.
    if (!hal_get_netif_started()) {
        ad2_printf_host(true, "%s daemon disabled. Network interface not enabled.", TAG);
        return;
    }

    bool en = false;
    ad2_get_config_key_bool(FTPD_CONFIG_SECTION, FTPD_SUBCMD_ENABLE, &en);

    // nothing more needs to be done once commands are set if not enabled.
    if (!en) {
        ad2_printf_host(true, "%s daemon disabled.", TAG);
        return;
    }

    ad2_printf_host(true, "%s: Init starting", TAG);

    // init the ftpd class.
    ad2ftpd = new FTPD();
    ad2ftpd->setCallbacks(new FTPDFileCallbacks());

    // load and parse ACL if set or set default to allow all.
    std::string acl = "0.0.0.0/0";
    ad2_get_config_key_string(FTPD_CONFIG_SECTION, FTPD_SUBCMD_ACL, acl);
    if (acl.length()) {
        int res = ad2ftpd_acl.add(acl);
        if (res != ad2ftpd_acl.ACL_FORMAT_OK) {
            ESP_LOGW(TAG, "ACL parse error %i for '%s'", res, acl.c_str());
        }
    }

    ad2_printf_host(true, "%s: Init done. Daemon starting.", TAG);
    xTaskCreate(&ftp_daemon_task, "ftp_daemon_task", 1024*8, NULL, tskIDLE_PRIORITY+1, NULL);

}
#endif /* CONFIG_AD2IOT_FTP_DAEMON */
