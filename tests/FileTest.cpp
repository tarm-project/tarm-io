/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "Timer.h"
#include "ScopeExitGuard.h"

#include "fs/File.h"
//#include "fs/Dir.h"

#include <boost/filesystem.hpp>

#include <cstdint>
#include <chrono>
#include <mutex>
#include <thread>


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

struct FileTest : public testing::Test,
                  public LogRedirector {
    FileTest() {
        m_tmp_test_dir = create_temp_test_directory();
    }

protected:
    std::string m_tmp_test_dir;
};

TEST_F(FileTest, default_constructor) {
    io::EventLoop loop;

    auto file = new io::fs::File(loop);
    ASSERT_FALSE(file->is_open());
    file->schedule_removal();

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(FileTest, open_existing) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;
    io::StatusCode open_status_code = io::StatusCode::UNDEFINED;

    auto file = new io::fs::File(loop);
    ASSERT_FALSE(file->is_open());
    EXPECT_EQ("", file->path());

    file->open(path, [&](io::fs::File& file, const io::Error& error) {
        ASSERT_TRUE(file.is_open());
        EXPECT_EQ(path, file.path());

        open_status_code = error.code();
    });
    EXPECT_FALSE(file->is_open());

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(io::StatusCode::OK, open_status_code);
    EXPECT_EQ(path, file->path());

    file->schedule_removal();
}

TEST_F(FileTest, DISABLED_double_open) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    bool opened_1 = false;
    bool opened_2 = false;

    auto file = new io::fs::File(loop);
    file->open(path, [&](io::fs::File& file, const io::Error& error) {
        opened_1 = true;
    });

    // TODO: fixme, data race possible here. The first open callback already triggered work in libuv

    // Overwriting callback and state, previous one will never be executed
    file->open(path, [&](io::fs::File& file, const io::Error& error) {
        opened_2 = true;
        file.schedule_removal();
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_FALSE(opened_1);
    EXPECT_TRUE(opened_2);

}

TEST_F(FileTest, open_in_open_callback) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    bool opened_1 = false;
    bool opened_2 = false;

    auto file = new io::fs::File(loop);
    file->open(path, [&](io::fs::File& file, const io::Error& error) {
        opened_1 = true;

        file.open(path, [&](io::fs::File& file, const io::Error& error) {
            opened_2 = true;
            file.schedule_removal();
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_TRUE(opened_1);
    EXPECT_TRUE(opened_2);

}

TEST_F(FileTest, open_not_existing) {
    const std::string path = m_tmp_test_dir + "/not_exist";

    io::EventLoop loop;

    bool opened = false;
    io::StatusCode status_code = io::StatusCode::UNDEFINED;

    auto file = new io::fs::File(loop);
    file->open(path, [&](io::fs::File& file, const io::Error& error) {
        EXPECT_FALSE(file.is_open());
        EXPECT_EQ(path, file.path());

        opened = !error;
        status_code = error.code();
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

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
    auto file = new io::fs::File(loop);
    file->open(existing_path, [&not_existing_path](io::fs::File& file, const io::Error& error) {
        EXPECT_FALSE(error);
        EXPECT_TRUE(file.is_open());

        file.open(not_existing_path, [](io::fs::File& file, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_FALSE(file.is_open());
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    file->schedule_removal();
}

TEST_F(FileTest, close_in_open_callback) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    auto file = new io::fs::File(loop);
    file->open(path, [&](io::fs::File& file, const io::Error& error) {
        EXPECT_TRUE(file.is_open());

        file.close();
        EXPECT_FALSE(file.is_open());
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    file->schedule_removal();
}

TEST_F(FileTest, close_not_open_file) {
    io::EventLoop loop;

    auto file = new io::fs::File(loop);
    ASSERT_FALSE(file->is_open());
    file->close();
    ASSERT_FALSE(file->is_open());

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    file->schedule_removal();
}

TEST_F(FileTest, double_close) {
    auto path = create_empty_file(m_tmp_test_dir);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    auto file = new io::fs::File(loop);
    file->open(path, [&](io::fs::File& file, const io::Error& error) {
        EXPECT_TRUE(file.is_open());

        file.close();
        EXPECT_FALSE(file.is_open());
        file.close();
        EXPECT_FALSE(file.is_open());
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    file->schedule_removal();
}

// TODO: open in read callback

TEST_F(FileTest, simple_read) {
    const std::size_t SIZE = 128;

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::StatusCode open_status_code = io::StatusCode::UNDEFINED;
    io::StatusCode read_status_code = io::StatusCode::UNDEFINED;
    bool end_read_called = false;

    io::EventLoop loop;
    auto file = new io::fs::File(loop);
    file->open(path, [&](io::fs::File& file, const io::Error& open_error) {
        open_status_code = open_error.code();

        if (open_error) {
            return;
        }

        file.read([&](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error) {
            read_status_code = read_error.code();
            ASSERT_EQ(SIZE, chunk.size);

            EXPECT_TRUE(file.is_open());

            for(std::size_t i = 0; i < SIZE / 4; i ++) {
                auto value = *reinterpret_cast<const std::uint32_t*>(chunk.buf.get() + (i * 4));
                ASSERT_EQ(i, value);
            }
        },
        [&](io::fs::File& file) {
            end_read_called = true;
            file.schedule_removal();
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    ASSERT_EQ(io::StatusCode::OK, open_status_code);
    ASSERT_EQ(io::StatusCode::OK, read_status_code);
    ASSERT_TRUE(end_read_called);
}

TEST_F(FileTest, reuse_callbacks_and_file_object) {
    const std::size_t SIZE_1 = 1024 * 2;
    const std::size_t SIZE_2 = 1024 * 32;

    auto path_1 = create_file_for_read(m_tmp_test_dir, SIZE_1);
    ASSERT_FALSE(path_1.empty());
    auto path_2 = create_file_for_read(m_tmp_test_dir, SIZE_2);
    ASSERT_FALSE(path_2.empty());

    std::function<void(io::fs::File&, const io::Error&)> open;

    auto read = [&](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error) {
        ASSERT_TRUE(!read_error);
        ASSERT_EQ(0, chunk.size % 4);

        for(std::size_t i = 0; i < chunk.size / 4; i ++) {
            auto read_value = *reinterpret_cast<const std::uint32_t*>(chunk.buf.get() + (i * 4));
            ASSERT_EQ(i + chunk.offset / 4, read_value) << " i=" << i;
        }
    };

    auto end_read = [&](io::fs::File& file) {
        if (file.path() == path_1) {
            file.close();
            file.open(path_2, open);
        } else {
            file.schedule_removal();
        }
    };

    open = [&](io::fs::File& file, const io::Error& open_error) {
        ASSERT_TRUE(!open_error);

        file.read(read, end_read);
    };

    io::EventLoop loop;
    auto file = new io::fs::File(loop);
    file->open(path_1, open);

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(FileTest, read_10mb_file) {
    const std::size_t SIZE = 10 * 1024 * 1024;
    const std::size_t READ_BUF_SIZE = io::fs::File::READ_BUF_SIZE;

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    size_t read_counter = 0;

    io::EventLoop loop;
    auto file = new io::fs::File(loop);
    file->open(path, [&](io::fs::File& file, const io::Error& open_error) {
        ASSERT_FALSE(open_error);

        file.read([&](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error) {
            ASSERT_EQ(READ_BUF_SIZE, chunk.size);

            for (size_t i = 0; i < chunk.size; i += 4) {
                auto value = *reinterpret_cast<const std::uint32_t*>(chunk.buf.get() + i);
                ASSERT_EQ(read_counter, value) << " read_counter= " << read_counter;

                ++read_counter;
            }
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    ASSERT_EQ(SIZE, read_counter * 4);

    file->schedule_removal();
}

// TODO: test schedure removal in open, read, ... callback

TEST_F(FileTest, read_not_open_file) {

    io::StatusCode read_status_code = io::StatusCode::UNDEFINED;
    bool end_read_called = false;

    io::EventLoop loop;
    auto file = new io::fs::File(loop);

    file->read([&](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error) {
        read_status_code = read_error.code();
    },
    [&](io::fs::File& file) {
        end_read_called = true;
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    ASSERT_EQ(io::StatusCode::FILE_NOT_OPEN, read_status_code);
    ASSERT_FALSE(end_read_called);

    file->schedule_removal();
}

TEST_F(FileTest, sequential_read_data_past_eof) {
    const std::size_t SIZE = 1024 * 4;

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    bool second_read_called = false;

    io::EventLoop loop;
    auto file = new io::fs::File(loop);
    file->open(path, [&second_read_called](io::fs::File& file, const io::Error& open_error) {
        ASSERT_TRUE(!open_error);

        file.read([](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error) {
            ASSERT_TRUE(!read_error);
        },
        [&second_read_called](io::fs::File& file) { // end read
            file.read([&second_read_called](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error) {
                second_read_called = true;
            });
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    ASSERT_FALSE(second_read_called);
    file->schedule_removal();
}

TEST_F(FileTest, close_in_read) {
    const std::size_t SIZE = 1024 * 512;

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;
    auto file = new io::fs::File(loop);

    std::size_t counter = 0;
    bool end_read_called = false;

    file->open(path, [&counter, &end_read_called](io::fs::File& file, const io::Error& open_error) {
        ASSERT_TRUE(!open_error);

        file.read([&counter](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error) {
            ASSERT_TRUE(!read_error);
            EXPECT_LE(counter, 5);

            if (counter == 5) {
                file.close();
                EXPECT_FALSE(file.is_open());
                return;
            }

            ++counter;
        },
        [&end_read_called](io::fs::File& file) {
            end_read_called = true;
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(5, counter);
    EXPECT_FALSE(file->is_open());
    EXPECT_FALSE(end_read_called);
    file->schedule_removal();
}

TEST_F(FileTest, DISABLED_read_sequential_of_closed_file) {
    const std::size_t SIZE = 1024 * 512;
    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    bool read_called = false;

    auto file = new io::fs::File(loop);
    file->open(path, [&](io::fs::File& file, const io::Error& error) {
        EXPECT_TRUE(!error);

        file.read([&](io::fs::File&, const io::DataChunk& chunk, const io::Error& error) {
            EXPECT_EQ(io::StatusCode::FILE_NOT_OPEN, error.code());

            read_called = true;
        });
        file.close(); // TODO: DATA race here because filesystem operations are performed in different threads
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_TRUE(read_called);

    file->schedule_removal();
}

// read in failed to opened file

TEST_F(FileTest, read_block) {
    const std::size_t SIZE = 128;
    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;
    auto file = new io::fs::File(loop);

    io::StatusCode read_status_code = io::StatusCode::UNDEFINED;

    file->open(path, [&](io::fs::File& file, const io::Error& open_error) {
        ASSERT_TRUE(!open_error);

        file.read_block(8, 16, [&](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error) {
            ASSERT_NE(nullptr, chunk.buf);
            ASSERT_EQ(8, chunk.offset);
            ASSERT_EQ(16, chunk.size);

            read_status_code = read_error.code();

            EXPECT_EQ(2, *reinterpret_cast<const std::uint32_t*>(chunk.buf.get() + 0));
            EXPECT_EQ(3, *reinterpret_cast<const std::uint32_t*>(chunk.buf.get() + 4));
            EXPECT_EQ(4, *reinterpret_cast<const std::uint32_t*>(chunk.buf.get() + 8));
            EXPECT_EQ(5, *reinterpret_cast<const std::uint32_t*>(chunk.buf.get() + 12));
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    ASSERT_EQ(io::StatusCode::OK, read_status_code);

    file->schedule_removal();
}

TEST_F(FileTest, read_block_past_edge) {
    const std::size_t SIZE = 128;
    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;
    auto file = new io::fs::File(loop);

    io::StatusCode read_status_code = io::StatusCode::UNDEFINED;

    file->open(path, [&](io::fs::File& file, const io::Error& open_error) {
        ASSERT_TRUE(!open_error);

        file.read_block(120, 16, [&](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error) {
            read_status_code = read_error.code();
            EXPECT_EQ(8, chunk.size);

            EXPECT_EQ(30, *reinterpret_cast<const std::uint32_t*>(chunk.buf.get() + 0));
            EXPECT_EQ(31, *reinterpret_cast<const std::uint32_t*>(chunk.buf.get() + 4));
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    ASSERT_EQ(io::StatusCode::OK, read_status_code);

    file->schedule_removal();
}

TEST_F(FileTest, DISABLED_read_block_not_existing_chunk) {
    // TODO: need to finish test

    const std::size_t SIZE = 128;
    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;
    auto file = new io::fs::File(loop);

    io::StatusCode read_status_code = io::StatusCode::UNDEFINED;

    file->open(path, [&](io::fs::File& file, const io::Error& open_error) {
        ASSERT_TRUE(!open_error);

        file.read_block(123456, 16, [&](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error) {
            // TODO: this is not called
            read_status_code = read_error.code();
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    ASSERT_EQ(io::StatusCode::OK, read_status_code);

    file->schedule_removal();
}

TEST_F(FileTest, read_block_not_opened) {
    io::EventLoop loop;

    io::StatusCode read_status = io::StatusCode::UNDEFINED;

    auto file = new io::fs::File(loop);
    file->read_block(0, 16, [&](io::fs::File& file, const io::DataChunk& chunk, const io::Error& error) {
        EXPECT_EQ(nullptr, chunk.buf);
        EXPECT_EQ(0, chunk.size);
        EXPECT_TRUE(error);

        read_status = error.code();
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    ASSERT_EQ(read_status, io::StatusCode::FILE_NOT_OPEN);

    file->schedule_removal();
}

// TODO: data race here between file read and close operations which are performed in separate threads each
TEST_F(FileTest, DISABLED_read_block_of_closed_file) {
    const std::size_t SIZE = 1024 * 512;
    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    bool read_called = false;

    auto file = new io::fs::File(loop);
    file->open(path, [&](io::fs::File& file, const io::Error& error) {
        EXPECT_TRUE(!error);

        file.read_block(1024, 1024, [&](io::fs::File&, const io::DataChunk& chunk, const io::Error& error) {
            EXPECT_EQ(io::StatusCode::FILE_NOT_OPEN, error.code());

            read_called = true;
        });
        file.close();
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_TRUE(read_called);

    file->schedule_removal();
}

TEST_F(FileTest, slow_read_data_consumer) {
    // Test description: in this test we are checking that read is paused if data recepient can not process it fast
    // Also loop should not exit despite that there are no active requests

    const std::size_t SIZE = io::fs::File::READ_BUF_SIZE * io::fs::File::READ_BUFS_NUM * 8;

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;

    std::vector<std::shared_ptr<const char>> captured_bufs;
    std::mutex mutex;
    bool exit_reseting_thread(false);

    // Using separate thread here instead of timer to not block event loop
    // and ensure if read is paused loop will not exit and continue to block on loop.run()
    std::thread t([&mutex, &captured_bufs, &exit_reseting_thread, &loop]() {
        size_t iterations_counter = 0;

        while (true) { // TODO: avoid potentially infinite loop
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            bool need_reset_bufs = false;

            {
                std::lock_guard<std::mutex> guard(mutex);

                if (captured_bufs.size() == io::fs::File::READ_BUFS_NUM || iterations_counter == 3 || exit_reseting_thread) {
                    iterations_counter = 0;
                    need_reset_bufs = true;
                }
            }

            if (need_reset_bufs) {
                // Modifying data of a loop is only allowed in the loop's thread, thereby we use execute_on_loop_thread
                loop.execute_on_loop_thread([&mutex, &captured_bufs](io::EventLoop& loop){
                    std::lock_guard<std::mutex> guard(mutex);
                    captured_bufs.clear();
                });
            }

            {
                std::lock_guard<std::mutex> guard(mutex);
                if (exit_reseting_thread && captured_bufs.empty()) {
                    break;
                }
            }

            ++iterations_counter;
        }
    });

    io::ScopeExitGuard scope_guard([&]() {
        t.join();
    });

    std::size_t bytes_read = 0;

    auto file = new io::fs::File(loop);
    file->open(path, [&captured_bufs, &mutex, &exit_reseting_thread, &bytes_read](io::fs::File& file, const io::Error& open_error) {
        ASSERT_TRUE(!open_error);

        file.read([&captured_bufs, &mutex, &bytes_read](io::fs::File& file,
                                                        const io::DataChunk& chunk,
                                                        const io::Error& read_error){
            ASSERT_TRUE(!read_error);
            bytes_read += chunk.size;

            std::lock_guard<std::mutex> guard(mutex);

            for(std::size_t i = 0; i < chunk.size / 4; i ++) {
                auto read_value = *reinterpret_cast<const std::uint32_t*>(chunk.buf.get() + (i * 4));
                ASSERT_EQ(i + chunk.offset / 4, read_value) << " i=" << i;
            }

            captured_bufs.push_back(chunk.buf);
        },
        [&mutex, &exit_reseting_thread](io::fs::File& file) {
            std::lock_guard<std::mutex> guard(mutex);
            exit_reseting_thread = true;
        });
    });

    // TODO: check from logger callback for message like "No free buffer found"

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    {
        std::lock_guard<std::mutex> guard(mutex);
        EXPECT_EQ(0, captured_bufs.size());
    }
    EXPECT_EQ(SIZE, bytes_read);

    {
        std::lock_guard<std::mutex> guard(mutex);
        exit_reseting_thread = true;
    }

    file->schedule_removal();
}

TEST_F(FileTest, schedule_file_removal_from_read) {
    // Test description: checking that File is removed only when all of its read buffers are released

    const std::size_t SIZE = io::fs::File::READ_BUF_SIZE; // space for one read operation

    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    bool end_read_called = false;
    std::shared_ptr<const char> captured_buf;

    bool file_buffers_in_use_event_occured = false;

    io::EventLoop loop;
    loop.enable_log([&file_buffers_in_use_event_occured](const std::string& msg){
        if (msg.find("File has read buffers in use, postponing removal") != std::string::npos) {
            file_buffers_in_use_event_occured = true;
        }
    });
    auto file = new io::fs::File(loop);

    std::mutex mutex;
    std::thread t([&mutex, &captured_buf, &loop]() {
        while (true) { // TODO: avoid potentially infinite loop
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            std::lock_guard<decltype(mutex)> guard(mutex);
            if (captured_buf) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // Modifying data of a loop is only allowed in the loop's thread, thereby we use async
                loop.execute_on_loop_thread([&captured_buf](io::EventLoop& loop){
                    captured_buf.reset();
                });
                break;
            }
        }
    });

    io::ScopeExitGuard scope_guard([&t]() {
        t.join();
    });

    file->open(path, [&end_read_called, &captured_buf, &mutex](io::fs::File& file, const io::Error& open_error) {
        ASSERT_TRUE(!open_error);

        file.read([&captured_buf, &mutex](io::fs::File& file, const io::DataChunk& chunk, const io::Error& read_error){
            {
                std::lock_guard<decltype(mutex)> guard(mutex);
                captured_buf = chunk.buf;
            }

            EXPECT_TRUE(file.is_open());
            file.schedule_removal();
            EXPECT_FALSE(file.is_open());
        },
        [&end_read_called](io::fs::File& file) {
            end_read_called = true;
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_FALSE(end_read_called);
    EXPECT_TRUE(file_buffers_in_use_event_occured);
}

TEST_F(FileTest, stat_size) {
    const std::size_t SIZE = 1236;
    auto path = create_file_for_read(m_tmp_test_dir, SIZE);
    ASSERT_FALSE(path.empty());

    std::size_t stat_call_count = 0;

    io::EventLoop loop;
    auto file = new io::fs::File(loop);
    file->open(path, [&SIZE, &stat_call_count](io::fs::File& file, const io::Error& error) {
        EXPECT_TRUE(!error);

        file.stat([&SIZE, &stat_call_count](io::fs::File& file, const io::fs::StatData& stat){
            EXPECT_EQ(SIZE, stat.size);
            ++stat_call_count;
        });
    });

    ASSERT_EQ(0, stat_call_count);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(0, stat_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    file->schedule_removal();
    ASSERT_EQ(1, stat_call_count);
}

TEST_F(FileTest, stat_time) {
    // Note: unfortunately file creation time may differ by 5-20msec from time get by std::chrono
    // or ::gettimeofday so using fuzzy comparison

    // Here usage of system_clock is essential for correct test run
    auto unix_time = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(unix_time).count();
    const auto nano_seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(unix_time).count() - seconds * 1000000000ll;

    auto path = create_file_for_read(m_tmp_test_dir, 16);
    ASSERT_FALSE(path.empty());

    io::EventLoop loop;
    auto file = new io::fs::File(loop);
    file->open(path, [&](io::fs::File& file, const io::Error& error) {
        EXPECT_TRUE(!error);

        file.stat([&](io::fs::File& file, const io::fs::StatData& stat){
            auto file_str = file.path().string().c_str();
            EXPECT_LE(std::abs(stat.last_access_time.seconds - seconds), 1l) << file_str;
            EXPECT_LE(std::abs(stat.last_access_time.nanoseconds - nano_seconds), 100000000l) << file_str;

            EXPECT_LE(std::abs(stat.last_modification_time.seconds - seconds), 1l) << file_str;
            EXPECT_LE(std::abs(stat.last_modification_time.nanoseconds - nano_seconds), 100000000l) << file_str;

            EXPECT_LE(std::abs(stat.last_status_change_time.seconds - seconds), 1l) << file_str;
            EXPECT_LE(std::abs(stat.last_status_change_time.nanoseconds - nano_seconds), 100000000l) << file_str;

            file.schedule_removal();
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

// TODO: more tests for various fields of StatData

TEST_F(FileTest, try_open_dir) {

    std::size_t on_open_call_count = 0;

    io::EventLoop loop;
    auto file = new io::fs::File(loop);
    file->open(m_tmp_test_dir, [&](io::fs::File& file, const io::Error& error) {
        EXPECT_TRUE(error);
        EXPECT_EQ(io::StatusCode::ILLEGAL_OPERATION_ON_A_DIRECTORY, error.code());
        ++on_open_call_count;
        file.schedule_removal();
    });

    EXPECT_EQ(0, on_open_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_open_call_count);
}

// TODO: test copy file larger than 4 GB
// For details see https://github.com/libuv/libuv/commit/2bbf7d5c8cd070cc8541698fe72136328bc18eae
