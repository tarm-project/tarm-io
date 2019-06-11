#include "UTCommon.h"

#include "io/File.h"
#include "io/Dir.h"
#include "io/Timer.h"
#include "io/ScopeExitGuard.h"

#include <cstdint>
#include <boost/filesystem.hpp>
#include <thread>
#include <mutex>

namespace {

std::string create_empty_file(const std::string& path_where_create) {
    std::string file_path = path_where_create + "/empty";
    std::ofstream ofile(file_path);
    if (ofile.fail()) {
        return "";
    }
    ofile.close();

    return file_path;
}

std::string create_file_for_read(const std::string& path_where_create, std::size_t size) {
    if (size % 4 != 0) {
        return "";
    }

    std::string file_path = path_where_create + "/file_" + std::to_string(size);
    std::ofstream ofile(file_path, std::ios::binary);
    if (ofile.fail()) {
        return "";
    }

    for (std::uint32_t i = 0; i < size / 4; ++i) {
        ofile.write(reinterpret_cast<char*>(&i), sizeof(i));
    }

    return file_path;
}

} // namespace

struct FileTest : public testing::Test {
    FileTest() {
        m_tmp_test_dir = create_temp_test_directory();
    }

protected:
    std::string m_tmp_test_dir;
};

TEST_F(FileTest, default_constructor) {
    io::EventLoop loop;

    auto file = new io::File(loop);
    ASSERT_FALSE(file->is_open());
    file->schedule_removal();
}

TEST_F(FileTest, open_existing) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;
    io::StatusCode open_status_code = io::StatusCode::UNDEFINED;

    auto file = new io::File(loop);
    ASSERT_FALSE(file->is_open());
    EXPECT_EQ("", file->path());

    file->open(path, [&](io::File& file, const io::Status& status) {
        ASSERT_TRUE(file.is_open());
        EXPECT_EQ(path, file.path());

        open_status_code = status.code();
    });
    EXPECT_FALSE(file->is_open());

    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(io::StatusCode::OK, open_status_code);
    EXPECT_EQ(path, file->path());

    file->schedule_removal();
}

TEST_F(FileTest, double_open) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    bool opened_1 = false;
    bool opened_2 = false;

    auto file = new io::File(loop);
    file->open(path, [&](io::File& file, const io::Status& status) {
        opened_1 = true;
    });

    // Overwriting callback and state, previous one will never be executed
    file->open(path, [&](io::File& file, const io::Status& status) {
        opened_2 = true;
        file.schedule_removal();
    });

    ASSERT_EQ(0, loop.run());

    EXPECT_FALSE(opened_1);
    EXPECT_TRUE(opened_2);

}

TEST_F(FileTest, open_in_open_callback) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    bool opened_1 = false;
    bool opened_2 = false;

    auto file = new io::File(loop);
    file->open(path, [&](io::File& file, const io::Status& status) {
        opened_1 = true;

        file.open(path, [&](io::File& file, const io::Status& status) {
            opened_2 = true;
            file.schedule_removal();
        });
    });

    ASSERT_EQ(0, loop.run());

    EXPECT_TRUE(opened_1);
    EXPECT_TRUE(opened_2);

}

TEST_F(FileTest, open_not_existing) {
    const std::string path = m_tmp_test_dir + "/not_exist";

    io::EventLoop loop;

    bool opened = false;
    io::StatusCode status_code = io::StatusCode::UNDEFINED;

    auto file = new io::File(loop);
    file->open(path, [&](io::File& file, const io::Status& status) {
        EXPECT_FALSE(file.is_open());
        EXPECT_EQ(path, file.path());

        opened = status.ok();
        status_code = status.code();
    });

    ASSERT_EQ(0, loop.run());

    ASSERT_FALSE(opened);
    EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, status_code);

    EXPECT_EQ("", file->path());
    file->schedule_removal();
}

TEST_F(FileTest, open_existing_open_not_existing) {
    auto existing_path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(existing_path.empty());

    const std::string not_existing_path = m_tmp_test_dir + "/not_exist";

    io::EventLoop loop;
    auto file = new io::File(loop);
    file->open(existing_path, [&not_existing_path](io::File& file, const io::Status& status) {
        EXPECT_TRUE(status.ok());
        EXPECT_TRUE(file.is_open());

        file.open(not_existing_path, [](io::File& file, const io::Status& status) {
            EXPECT_FALSE(status.ok());
            EXPECT_FALSE(file.is_open());
        });
    });

    ASSERT_EQ(0, loop.run());
    file->schedule_removal();
}

TEST_F(FileTest, close_in_open_callback) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    auto file = new io::File(loop);
    file->open(path, [&](io::File& file, const io::Status& status) {
        EXPECT_TRUE(file.is_open());

        file.close();
        EXPECT_FALSE(file.is_open());
    });

    ASSERT_EQ(0, loop.run());
    file->schedule_removal();
}

TEST_F(FileTest, close_not_open_file) {
    io::EventLoop loop;

    auto file = new io::File(loop);
    ASSERT_FALSE(file->is_open());
    file->close();
    ASSERT_FALSE(file->is_open());

    ASSERT_EQ(0, loop.run());
    file->schedule_removal();
}

TEST_F(FileTest, double_close) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    auto file = new io::File(loop);
    file->open(path, [&](io::File& file, const io::Status& status) {
        EXPECT_TRUE(file.is_open());

        file.close();
        EXPECT_FALSE(file.is_open());
        file.close();
        EXPECT_FALSE(file.is_open());
    });

    ASSERT_EQ(0, loop.run());
    file->schedule_removal();
}

TEST_F(FileTest, simple_read) {
    const std::size_t SIZE = 128;

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::StatusCode open_status_code = io::StatusCode::UNDEFINED;
    io::StatusCode read_status_code = io::StatusCode::UNDEFINED;
    bool end_read_called = false;

    io::EventLoop loop;
    auto file = new io::File(loop);
    file->open(path, [&](io::File& file, const io::Status& open_status) {
        open_status_code = open_status.code();

        if (open_status.fail()) {
            return;
        }

        file.read([&](io::File& file, std::shared_ptr<const char> buf, std::size_t size, const io::Status& read_status) {
            read_status_code = read_status.code();
            ASSERT_EQ(SIZE, size);

            EXPECT_TRUE(file.is_open());

            for(std::size_t i = 0; i < SIZE / 4; i ++) {
                auto value = *reinterpret_cast<const std::uint32_t*>(buf.get() + (i * 4));
                ASSERT_EQ(i, value);
            }
        },
        [&](io::File& file) {
            end_read_called = true;
            file.schedule_removal();
        });
    });

    ASSERT_EQ(0, loop.run());

    ASSERT_EQ(io::StatusCode::OK, open_status_code);
    ASSERT_EQ(io::StatusCode::OK, read_status_code);
    ASSERT_TRUE(end_read_called);
}

TEST_F(FileTest, read_10mb_file) {
    const std::size_t SIZE = 10 * 1024 * 1024;
    const std::size_t READ_BUF_SIZE = io::File::READ_BUF_SIZE;

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    size_t read_counter = 0;

    io::EventLoop loop;
    auto file = new io::File(loop);
    file->open(path, [&](io::File& file, const io::Status& open_status) {
        ASSERT_FALSE(open_status.fail());

        file.read([&](io::File& file, std::shared_ptr<const char> buf, std::size_t size, const io::Status& read_status) {
            ASSERT_EQ(READ_BUF_SIZE, size);

            for (size_t i = 0; i < size; i += 4) {
                auto value = *reinterpret_cast<const std::uint32_t*>(buf.get() + i);
                ASSERT_EQ(read_counter, value) << " read_counter= " << read_counter;

                ++read_counter;
            }
        });
    });

    ASSERT_EQ(0, loop.run());
    ASSERT_EQ(SIZE, read_counter * 4);

    file->schedule_removal();
}

// TODO: test schedure removal in open, read, ... callback

TEST_F(FileTest, read_not_open_file) {

    io::StatusCode read_status_code = io::StatusCode::UNDEFINED;
    bool end_read_called = false;

    io::EventLoop loop;
    auto file = new io::File(loop);

    file->read([&](io::File& file, std::shared_ptr<const char> buf, std::size_t size, const io::Status& read_status) {
        read_status_code = read_status.code();
    },
    [&](io::File& file) {
        end_read_called = true;
    });

    ASSERT_EQ(0, loop.run());

    ASSERT_EQ(io::StatusCode::FILE_NOT_OPEN, read_status_code);
    ASSERT_FALSE(end_read_called);

    file->schedule_removal();
}

TEST_F(FileTest, close_in_read) {
    const std::size_t SIZE = 1024 * 512;

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;
    auto file = new io::File(loop);

    std::size_t counter = 0;
    bool end_read_called = false;

    file->open(path, [&counter, &end_read_called](io::File& file, const io::Status& open_status) {
        ASSERT_TRUE(open_status.ok());

        file.read([&counter](io::File& file, std::shared_ptr<const char> buf, std::size_t size, const io::Status& read_status) {
            ASSERT_TRUE(read_status.ok());
            EXPECT_LE(counter, 5);

            if (counter == 5) {
                file.close();
                EXPECT_FALSE(file.is_open());
                return;
            }

            ++counter;
        },
        [&end_read_called](io::File& file) {
            end_read_called = true;
        });
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(5, counter);
    EXPECT_FALSE(file->is_open());
    EXPECT_FALSE(end_read_called);
    file->schedule_removal();
}

// read in failed to opened file

TEST_F(FileTest, read_block) {
    const std::size_t SIZE = 128;

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;
    auto file = new io::File(loop);

    io::StatusCode read_status_code = io::StatusCode::UNDEFINED;

    file->open(path, [&](io::File& file, const io::Status& open_status) {
        ASSERT_TRUE(open_status.ok());

        file.read_block(8, 16, [&](io::File& file, std::shared_ptr<const char> buf, std::size_t size, const io::Status& read_status) {
            ASSERT_NE(nullptr, buf);
            ASSERT_EQ(16, size);

            read_status_code = read_status.code();

            EXPECT_EQ(2, *reinterpret_cast<const std::uint32_t*>(buf.get() + 0));
            EXPECT_EQ(3, *reinterpret_cast<const std::uint32_t*>(buf.get() + 4));
            EXPECT_EQ(4, *reinterpret_cast<const std::uint32_t*>(buf.get() + 8));
            EXPECT_EQ(5, *reinterpret_cast<const std::uint32_t*>(buf.get() + 12));
        });
    });

    ASSERT_EQ(0, loop.run());

    ASSERT_EQ(io::StatusCode::OK, read_status_code);

    file->schedule_removal();
}

TEST_F(FileTest, read_block_not_opened) {
    io::EventLoop loop;

    io::StatusCode read_status = io::StatusCode::UNDEFINED;

    auto file = new io::File(loop);
    file->read_block(0, 16, [&](io::File& file, std::shared_ptr<const char> buf, std::size_t size, const io::Status& status) {
        EXPECT_EQ(nullptr, buf);
        EXPECT_EQ(0, size);
        EXPECT_FALSE(status.ok());

        read_status = status.code();
    });

    ASSERT_EQ(0, loop.run());
    ASSERT_EQ(read_status, io::StatusCode::FILE_NOT_OPEN);

    file->schedule_removal();
}

TEST_F(FileTest, slow_read_data_consumer) {
    // Test description: in this test we are checking that read is paused if data recepient can not process it fast
    // Also loop should not exit despite that there are no active requests

    const std::size_t SIZE = io::File::READ_BUF_SIZE * io::File::READ_BUFS_NUM * 8;

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    std::vector<std::shared_ptr<const char>> captured_bufs;
    std::mutex mutex;
    bool exit_reseting_thread = false;

    std::size_t bytes_read = 0;

    // Using separate thread here instead of timer to not block event loop
    // and ensure if read is paused loop will not exit and continue to block on loop.run()
    std::thread t([&mutex, &captured_bufs, &exit_reseting_thread]() {
        size_t iterations_counter = 0;

        while (true) { // TODO: avoid potentially infinite loop
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            std::lock_guard<std::mutex> guard(mutex);
            if (exit_reseting_thread) {
                break;
            }

            if (captured_bufs.size() == io::File::READ_BUFS_NUM || iterations_counter == 3) {
                iterations_counter = 0;
                captured_bufs.clear();
            }

            ++iterations_counter;
        }
    });

    io::ScopeExitGuard scope_guard([&]() {
        t.join();
    });

    auto file = new io::File(loop);
    file->open(path, [&captured_bufs, &mutex, &exit_reseting_thread, &bytes_read](io::File& file, const io::Status& open_status) {
        ASSERT_TRUE(open_status.ok());

        file.read([&captured_bufs, &mutex, &bytes_read](io::File& file,
                                                        std::shared_ptr<const char> buf,
                                                        std::size_t size,
                                                        const io::Status& read_status){
            ASSERT_TRUE(read_status.ok());

            bytes_read += size;

            std::lock_guard<std::mutex> guard(mutex);
            captured_bufs.push_back(buf);
        },
        [&mutex, &exit_reseting_thread](io::File& file) {
            std::lock_guard<std::mutex> guard(mutex);
            exit_reseting_thread = true;
        });
    });

    // TODO: check from logger callback thatfor message like "No free buffer found"

    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(SIZE, bytes_read);

    {
        std::lock_guard<std::mutex> guard(mutex);
        exit_reseting_thread = true;
    }

    file->schedule_removal();
}

TEST_F(FileTest, schedule_file_removal_from_read) {
    // Test description: checking that File is removed only when all of its read buffers are released

    const std::size_t SIZE = io::File::READ_BUF_SIZE; // space for one read operation

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    bool end_read_called = false;
    std::shared_ptr<const char> captured_buf;

    io::EventLoop loop;
    auto file = new io::File(loop);

    std::mutex mutex;
    std::thread t([&mutex, &captured_buf, file]() {
        while (true) { // TODO: avoid potentially infinite loop
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            std::lock_guard<decltype(mutex)> guard(mutex);
            if (captured_buf) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                EXPECT_FALSE(file->is_open());
                captured_buf.reset();
                break;
            }
        }
    });

    io::ScopeExitGuard scope_guard([&t]() {
        t.join();
    });

    file->open(path, [&end_read_called, &captured_buf, &mutex](io::File& file, const io::Status& open_status) {
        ASSERT_TRUE(open_status.ok());

        file.read([&captured_buf, &mutex](io::File& file, std::shared_ptr<const char> buf, std::size_t size, const io::Status& read_status){
            {
                std::lock_guard<decltype(mutex)> guard(mutex);
                captured_buf = buf;
            }

            EXPECT_TRUE(file.is_open());
            file.schedule_removal();
            EXPECT_FALSE(file.is_open());
        },
        [&end_read_called](io::File& file) {
            end_read_called = true;
        });
    });

    // TODO: check from logger callback thatfor message like "No free buffer found"

    ASSERT_EQ(0, loop.run());
    EXPECT_FALSE(end_read_called);
}


// TODO: open file wich is dir
