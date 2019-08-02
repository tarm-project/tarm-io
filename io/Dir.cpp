#include "Dir.h"

#include <cstring>
#include <assert.h>

namespace io {

class Dir::Impl {
public:
    Impl(EventLoop& loop, Dir& parent);
    ~Impl();

    void open(const std::string& path, OpenCallback callback);
    bool is_open() const;
    void close();

    void read(ReadCallback read_callback, EndReadCallback end_read_callback = nullptr);

    //void schedule_removal() override;

    const std::string& path() const;

protected:
    // statics
    static void on_open_dir(uv_fs_t* req);
    static void on_read_dir(uv_fs_t* req);

private:
    static constexpr std::size_t DIRENTS_NUMBER = 1;

    Dir* m_parent = nullptr;
    uv_loop_t* m_uv_loop;

    OpenCallback m_open_callback = nullptr;
    ReadCallback m_read_callback = nullptr;
    EndReadCallback m_end_read_callback = nullptr;

    std::string m_path;

    uv_fs_t m_open_dir_req;
    uv_fs_t m_read_dir_req;
    uv_dir_t* m_uv_dir = nullptr;

    uv_dirent_t m_dirents[DIRENTS_NUMBER];
};

namespace {

DirectoryEntryType convert_direntry_type(uv_dirent_type_t type) {
    switch (type) {
        case UV_DIRENT_FILE:
            return DirectoryEntryType::FILE;
        case UV_DIRENT_DIR:
            return DirectoryEntryType::DIR;
        case UV_DIRENT_LINK:
            return DirectoryEntryType::LINK;
        case UV_DIRENT_FIFO:
            return DirectoryEntryType::FIFO;
        case UV_DIRENT_SOCKET:
            return DirectoryEntryType::SOCKET;
        case UV_DIRENT_CHAR:
            return DirectoryEntryType::CHAR;
        case UV_DIRENT_BLOCK:
            return DirectoryEntryType::BLOCK;
        case UV_DIRENT_UNKNOWN:
        default:
            return DirectoryEntryType::UNKNOWN;
    }

    return DirectoryEntryType::UNKNOWN;
}

}

const std::string& Dir::Impl::path() const {
    return m_path;
}

Dir::Impl::Impl(EventLoop& loop, Dir& parent) :
    m_parent(&parent),
    m_uv_loop(reinterpret_cast<uv_loop_t*>(loop.raw_loop())) {
    memset(&m_open_dir_req, 0, sizeof(m_open_dir_req));
    memset(&m_read_dir_req, 0, sizeof(m_read_dir_req));
}

Dir::Impl::~Impl() {
}

void Dir::Impl::open(const std::string& path, OpenCallback callback) {
    m_path = path;
    m_open_callback = callback;
    m_open_dir_req.data = this;
    uv_fs_opendir(m_uv_loop, &m_open_dir_req, path.c_str(), on_open_dir);
}

void Dir::Impl::read(ReadCallback read_callback, EndReadCallback end_read_callback) {
    // TODO: check if open
    m_read_dir_req.data = this;
    m_read_callback = read_callback;
    m_end_read_callback = end_read_callback;

    uv_fs_readdir(m_uv_loop, &m_read_dir_req, m_uv_dir, on_read_dir);
}

bool Dir::Impl::is_open() const {
    return m_uv_dir != nullptr;
}

void Dir::Impl::close() {
    if (!is_open()) { // did not open // TODO: revise this for case when open occured with error
        return;
    }

    m_path.clear();

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(m_open_dir_req.ptr);

    uv_fs_req_cleanup(&m_open_dir_req);
    uv_fs_req_cleanup(&m_read_dir_req);

    m_open_dir_req.data = nullptr;
    m_read_dir_req.data = nullptr;

    // synchronous
    uv_fs_t closedir_req;
    uv_fs_closedir(m_uv_loop, &closedir_req, dir, nullptr);

    m_uv_dir = nullptr;
}

////////////////////////////////////////////// static //////////////////////////////////////////////
void Dir::Impl::on_open_dir(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<Dir::Impl*>(req->data);

    if (req->result < 0 ) {
        uv_fs_req_cleanup(&this_.m_open_dir_req);

        //std::cerr << "Failed to open dir: " << req->path << std::endl;
        // TODO: error handling
    } else {
        this_.m_uv_dir = reinterpret_cast<uv_dir_t*>(req->ptr);
        this_.m_uv_dir->dirents = this_.m_dirents;
        this_.m_uv_dir->nentries = Dir::Impl::DIRENTS_NUMBER;
    }

    if (this_.m_open_callback) {
        Status status(req->result);
        this_.m_open_callback(*this_.m_parent, status);
    }

    if (req->result < 0 ) { // TODO: replace with if status.fail()
        this_.m_path.clear();
    }
}

void Dir::Impl::on_read_dir(uv_fs_t* req) {
    auto& this_ = *reinterpret_cast<Dir::Impl*>(req->data);

    uv_dir_t* dir = reinterpret_cast<uv_dir_t*>(req->ptr);

    if (req->result == 0) {
        if (this_.m_end_read_callback) {
            this_.m_end_read_callback(*this_.m_parent);
        }
    } else {
        if (this_.m_read_callback) {
            this_.m_read_callback(*this_.m_parent, this_.m_dirents[0].name, convert_direntry_type(this_.m_dirents[0].type));
        }

        uv_fs_req_cleanup(&this_.m_read_dir_req); // cleaning up previous request
        uv_fs_readdir(req->loop, &this_.m_read_dir_req, dir, on_read_dir);
    }
}

///////////////////////////////////////// implementation ///////////////////////////////////////////

Dir::Dir(EventLoop& loop) :
    Disposable(loop),
    m_impl(new Impl(loop, *this)) {
}

Dir::~Dir() {
}

void Dir::open(const std::string& path, OpenCallback callback) {
    return m_impl->open(path, callback);
}

bool Dir::is_open() const {
    return m_impl->is_open();
}

void Dir::close() {
    return m_impl->close();
}

void Dir::read(ReadCallback read_callback, EndReadCallback end_read_callback) {
    return m_impl->read(read_callback, end_read_callback);
}

const std::string& Dir::path() const {
    return m_impl->path();
}

void Dir::schedule_removal() {
    m_impl->close();

    Disposable::schedule_removal();
}

/////////////////////////////////////////// functions //////////////////////////////////////////////

template<typename CallbackType>
struct RequestWithCallback : public uv_fs_t {
    RequestWithCallback(CallbackType c) :
        callback(c) {
    }

    CallbackType callback = nullptr;
};

void on_make_temp_dir(uv_fs_t* uv_request) {
    auto& request = *reinterpret_cast<RequestWithCallback<MakeTempDirCallback>*>(uv_request);

    if (request.callback) {
        request.callback(request.path, Status(request.result));
    }

    uv_fs_req_cleanup(uv_request);
    delete &request;
}

void make_temp_dir(EventLoop& loop, const std::string& name_template, MakeTempDirCallback callback) {
    auto request = new RequestWithCallback<MakeTempDirCallback>(callback);
    const Status status = uv_fs_mkdtemp(reinterpret_cast<uv_loop_t*>(loop.raw_loop()), request, name_template.c_str(), on_make_temp_dir);
    if (status.fail()) {
        if (callback) {
            callback("", status);
        }

        delete request;
    }
}

void on_make_dir(uv_fs_t* uv_request) {
    auto& request = *reinterpret_cast<RequestWithCallback<MakeDirCallback>*>(uv_request);

    if (request.callback) {
        request.callback(Status(request.result));
    }

    uv_fs_req_cleanup(uv_request);
    delete &request;
}

void make_dir(EventLoop& loop, const std::string& path, MakeDirCallback callback) {
    auto request = new RequestWithCallback<MakeDirCallback>(callback);
    const Status status = uv_fs_mkdtemp(reinterpret_cast<uv_loop_t*>(loop.raw_loop()), request, path.c_str(), on_make_dir);
    if (status.fail()) {
        if (callback) {
            callback(status);
        }

        delete request;
    }
}

} // namespace io
