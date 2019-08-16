
#include "UTCommon.h"

#include "io/Dir.h"

#include <boost/filesystem.hpp>

#include <vector>
#include <thread>

// TOOD: what about Windows here????
#include <sys/stat.h>
//#include <sys/sysmacros.h>

struct DirTest : public testing::Test,
                 public LogRedirector {
    DirTest() {
        m_tmp_test_dir = create_temp_test_directory();
    }

protected:
    boost::filesystem::path m_tmp_test_dir;
};

TEST_F(DirTest, default_constructor) {
    io::EventLoop loop;

    auto dir = new io::Dir(loop);
    EXPECT_TRUE(dir->path().empty());
    EXPECT_FALSE(dir->is_open());

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();
}


TEST_F(DirTest, open_then_close) {
    io::EventLoop loop;
    auto dir = new io::Dir(loop);

    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status& status) {
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(io::StatusCode::OK, status.code());

        EXPECT_EQ(m_tmp_test_dir, dir.path());
        EXPECT_TRUE(dir.is_open());
        dir.close();
        EXPECT_TRUE(dir.path().empty());
        EXPECT_FALSE(dir.is_open());
    });
    EXPECT_FALSE(dir->is_open());

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();
}

// TODO: open dir which is file

TEST_F(DirTest, open_not_existing) {
    io::EventLoop loop;
    auto dir = new io::Dir(loop);

    bool callback_called = false;

    auto path = (m_tmp_test_dir / "not_exists").string();
    dir->open(path, [&](io::Dir& dir, const io::Status& status) {
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, status.code());

        callback_called = true;

        // TODO: error check here
        EXPECT_EQ(path, dir.path());
        EXPECT_FALSE(dir.is_open());
    });

    ASSERT_EQ(0, loop.run());

    EXPECT_TRUE(callback_called);
    EXPECT_EQ("", dir->path());
    dir->schedule_removal();
}

TEST_F(DirTest, list_elements) {
    boost::filesystem::create_directories(m_tmp_test_dir/ "dir_1");
    boost::filesystem::create_directories(m_tmp_test_dir/ "dir_2");
    {
        std::ofstream ofile((m_tmp_test_dir/ "file_1").string());
        ASSERT_FALSE(ofile.fail());
    }
    boost::filesystem::create_directories(m_tmp_test_dir/ "dir_3");
    {
        std::ofstream ofile((m_tmp_test_dir/ "file_2").string());
        ASSERT_FALSE(ofile.fail());
    }

    bool dir_1_listed = false;
    bool dir_2_listed = false;
    bool dir_3_listed = false;
    bool file_1_listed = false;
    bool file_2_listed = false;

    io::EventLoop loop;
    auto dir = new io::Dir(loop);
    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status&) {
        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            if (std::string(name) == "dir_1") {
                EXPECT_FALSE(dir_1_listed);
                EXPECT_EQ(io::DirectoryEntryType::DIR, entry_type);
                dir_1_listed = true;
            } else if (std::string(name) == "dir_2") {
                EXPECT_FALSE(dir_2_listed);
                EXPECT_EQ(io::DirectoryEntryType::DIR, entry_type);
                dir_2_listed = true;
            } else if (std::string(name) == "dir_3") {
                EXPECT_FALSE(dir_3_listed);
                EXPECT_EQ(io::DirectoryEntryType::DIR, entry_type);
                dir_3_listed = true;
            } else if (std::string(name) == "file_1") {
                EXPECT_FALSE(file_1_listed);
                EXPECT_EQ(io::DirectoryEntryType::FILE, entry_type);
                file_1_listed = true;
            } else if (std::string(name) == "file_2") {
                EXPECT_FALSE(file_2_listed);
                EXPECT_EQ(io::DirectoryEntryType::FILE, entry_type);
                file_2_listed = true;
            }
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_TRUE(dir_1_listed);
    EXPECT_TRUE(dir_2_listed);
    EXPECT_TRUE(dir_3_listed);
    EXPECT_TRUE(file_1_listed);
    EXPECT_TRUE(file_2_listed);
}

TEST_F(DirTest, close_in_list_callback) {
    // TODO:
}

TEST_F(DirTest, empty_dir) {
    io::EventLoop loop;
    auto dir = new io::Dir(loop);

    // TODO: rename???? to "list"
    bool read_called = false;
    bool end_read_called = false;

    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status&) {
        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            read_called = true;
        }, // end_read
        [&](io::Dir& dir) {
            end_read_called = true;
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_FALSE(read_called);
    EXPECT_TRUE(end_read_called);
}

TEST_F(DirTest, no_read_callback) {
    {
        std::ofstream ofile((m_tmp_test_dir/ "some_file").string());
        ASSERT_FALSE(ofile.fail());
    }

    io::EventLoop loop;
    auto dir = new io::Dir(loop);

    // TODO: rename???? to "list"
    bool end_read_called = false;

    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status&) {
        dir.read(nullptr,
        [&](io::Dir& dir) { // end_read
            end_read_called = true;
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_TRUE(end_read_called);
}

TEST_F(DirTest, list_symlink) {
    auto file_path = m_tmp_test_dir / "some_file";
    {
        std::ofstream ofile(file_path.string());
        ASSERT_FALSE(ofile.fail());
    }

    boost::filesystem::create_symlink(file_path, m_tmp_test_dir / "link");

    std::size_t entries_count = 0;
    bool link_found = false;

    io::EventLoop loop;
    auto dir = new io::Dir(loop);
    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status&) {
        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            if (std::string(name) == "some_file") {
                EXPECT_EQ(io::DirectoryEntryType::FILE, entry_type);
            } else if (std::string(name) == "link") {
                EXPECT_EQ(io::DirectoryEntryType::LINK, entry_type);
                link_found = true;
            }

            ++entries_count;
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_TRUE(link_found);
    EXPECT_EQ(2, entries_count);
}

#if defined(__APPLE__) || defined(__linux__)
TEST_F(DirTest, list_block_and_char_devices) {
    std::size_t block_devices_count = 0;
    std::size_t char_devices_count = 0;

    io::EventLoop loop;
    auto dir = new io::Dir(loop);
    dir->open("/dev", [&](io::Dir& dir, const io::Status& status) {
        ASSERT_TRUE(status.ok());

        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            if (entry_type == io::DirectoryEntryType::BLOCK) {
                ++block_devices_count;
            } else if (entry_type == io::DirectoryEntryType::CHAR) {
                ++char_devices_count;
            }
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_GT(block_devices_count, 0);
    EXPECT_GT(char_devices_count, 0);
}

TEST_F(DirTest, list_domain_sockets) {
    std::size_t domain_sockets_count = 0;

    io::EventLoop loop;
    auto dir = new io::Dir(loop);
    dir->open("/var/run", [&](io::Dir& dir, const io::Status& status) {
        ASSERT_TRUE(status.ok());

        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            if (entry_type == io::DirectoryEntryType::SOCKET) {
                ++domain_sockets_count;
            }
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_GT(domain_sockets_count, 0);
}

TEST_F(DirTest, list_fifo) {
    std::size_t fifo_count = 0;

    auto fifo_status = mkfifo((m_tmp_test_dir / "fifo").string().c_str(), S_IRWXU);
    ASSERT_EQ(0, fifo_status);

    io::EventLoop loop;
    auto dir = new io::Dir(loop);
    dir->open(m_tmp_test_dir.string(), [&](io::Dir& dir, const io::Status& status) {
        ASSERT_TRUE(status.ok());

        dir.read([&](io::Dir& dir, const char* name, io::DirectoryEntryType entry_type) {
            if (entry_type == io::DirectoryEntryType::FIFO) {
                ++fifo_count;
            }
        });
    });

    ASSERT_EQ(0, loop.run());
    dir->schedule_removal();

    EXPECT_EQ(1, fifo_count);
}

#endif

TEST_F(DirTest, make_temp_dir) {
    io::EventLoop loop;

    bool callback_called = false;

    const std::string template_path = (m_tmp_test_dir / "temp-XXXXXX").string();

    io::make_temp_dir(loop, template_path,
        [&](const std::string& dir, const io::Status& status) {
            callback_called = true;
            EXPECT_TRUE(status.ok());
            EXPECT_EQ(template_path.size(), dir.size());
            EXPECT_EQ(0, dir.find((m_tmp_test_dir / "temp-").string()));
            EXPECT_TRUE(boost::filesystem::exists(dir));
        }
    );

    EXPECT_FALSE(callback_called); // Checking that operation is really async
    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(callback_called);
}

// TOOD: this test is passing on OS X. Need to hide pattern inside the call
TEST_F(DirTest, DISABLED_make_temp_dir_invalid_template) {
    io::EventLoop loop;

    bool callback_called = false;

    // There should be 6 'X' chars
    const std::string template_path = (m_tmp_test_dir / "temp-XXXXX").string();

    io::make_temp_dir(loop, template_path,
        [&](const std::string& dir, const io::Status& status) {
            callback_called = true;
            EXPECT_TRUE(status.fail());
            EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, status.code());
        }
    );

    EXPECT_FALSE(callback_called);
    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(callback_called);
}

// TODO: directory creation permissions
TEST_F(DirTest, make_dir) {
    io::EventLoop loop;

    bool callback_called = false;

    const std::string path = (m_tmp_test_dir / "new_dir").string();

    io::make_dir(loop, path,
        [&](const io::Status& status) {
            callback_called = true;
            EXPECT_TRUE(status.ok());
            EXPECT_TRUE(boost::filesystem::exists(path));
        }
    );

    EXPECT_FALSE(callback_called);
    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(callback_called);
}

TEST_F(DirTest, make_dir_no_such_dir_error) {
    io::EventLoop loop;

    bool callback_called = false;

    const std::string path = (m_tmp_test_dir / "no_exists" / "new_dir").string();

    io::make_dir(loop, path,
        [&](const io::Status& status) {
            callback_called = true;
            EXPECT_TRUE(status.fail());
            EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, status.code());
        }
    );

    EXPECT_FALSE(callback_called);
    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(callback_called);
}

TEST_F(DirTest, make_dir_exists_error) {
    io::EventLoop loop;

    bool callback_called = false;

    const std::string path = (m_tmp_test_dir).string();

    io::make_dir(loop, path,
        [&](const io::Status& status) {
            callback_called = true;
            EXPECT_TRUE(status.fail());
            EXPECT_EQ(io::StatusCode::FILE_OR_DIR_ALREADY_EXISTS, status.code());
        }
    );

    EXPECT_FALSE(callback_called);
    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(callback_called);
}

TEST_F(DirTest, make_dir_empty_path_error) {
    io::EventLoop loop;

    bool callback_called = false;

    io::make_dir(loop, "",
        [&](const io::Status& status) {
            callback_called = true;
            EXPECT_TRUE(status.fail());
            // TODO:
            // On Linux io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY is returned here
            //EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, status.code());
        }
    );

    EXPECT_FALSE(callback_called);
    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(callback_called);
}

TEST_F(DirTest, make_dir_name_to_long_error) {
    io::EventLoop loop;

    bool callback_called = false;

    const std::string path = (m_tmp_test_dir / "1234567890qwertyuiopasdfgghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm").string();

    io::make_dir(loop, path,
        [&](const io::Status& status) {
            callback_called = true;
            EXPECT_TRUE(status.fail());
            EXPECT_EQ(io::StatusCode::NAME_TOO_LONG, status.code());
        }
    );

    EXPECT_FALSE(callback_called);
    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(callback_called);
}

// TODO: check for error if creating without write permission in path components

#if defined(__APPLE__) || defined(__linux__)
TEST_F(DirTest, make_dir_root_dir_error) {
    io::EventLoop loop;

    bool callback_called = false;

    io::make_dir(loop, "/",
        [&](const io::Status& status) {
            callback_called = true;
            EXPECT_TRUE(status.fail());
            // TODO:
            // On Linux another error: io::StatusCode::FILE_OR_DIR_ALREADY_EXISTS
            //EXPECT_EQ(io::StatusCode::ILLEGAL_OPERATION_ON_A_DIRECTORY, status.code());
        }
    );

    EXPECT_FALSE(callback_called);
    ASSERT_EQ(0, loop.run());
    EXPECT_TRUE(callback_called);
}
#endif

TEST_F(DirTest, remove_dir) {
    // TODO: need to check if directories creation succeeded
    boost::filesystem::create_directories(m_tmp_test_dir / "a" / "b" / "c" / "d");
    boost::filesystem::create_directories(m_tmp_test_dir / "a" / "b" / "c" / "e");
    boost::filesystem::create_directories(m_tmp_test_dir / "a" / "b" / "f");
    boost::filesystem::create_directories(m_tmp_test_dir / "a" / "g");
    boost::filesystem::create_directories(m_tmp_test_dir / "h");
    boost::filesystem::create_directories(m_tmp_test_dir / "i" / "j");
    boost::filesystem::create_directories(m_tmp_test_dir / "i" / "k");
    boost::filesystem::create_directories(m_tmp_test_dir / "i" / "l");
    boost::filesystem::create_directories(m_tmp_test_dir / "i" / "m" / "n");
    boost::filesystem::create_directories(m_tmp_test_dir / "o" / "p");

    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "root_1").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "root_2").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "a" / "a_1").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "a" / "a_2").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "a" / "b" / "b_1").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "a" / "b" / "c" / "c_1").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "a" / "b" / "c" / "c_2").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "a" / "b" / "c" / "e" / "e_1").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "i" / "i_1").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "i" / "m" / "n" / "n_1").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "i" / "l" / "l_1").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "i" / "l" / "l_2").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "i" / "l" / "l_3").string()).fail());

    int callback_call_count = 0;

    io::EventLoop loop;

    io::remove_dir(loop, m_tmp_test_dir.string(), [&](const io::Status& status) {
        ++callback_call_count;
        EXPECT_TRUE(status.ok());
        EXPECT_FALSE(boost::filesystem::exists(m_tmp_test_dir));
    });

    EXPECT_EQ(0, callback_call_count);
    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(1, callback_call_count);
}

TEST_F(DirTest, remove_dir_with_progress) {
    // TODO: need to check if directories creation succeeded
    boost::filesystem::create_directories(m_tmp_test_dir / "a" / "b" / "c");
    boost::filesystem::create_directories(m_tmp_test_dir / "a" / "e" / "f");
    boost::filesystem::create_directories(m_tmp_test_dir / "h");

    // Files are not tracked by progress
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "root_1").string()).fail());
    ASSERT_FALSE(std::ofstream((m_tmp_test_dir / "a" / "a_1").string()).fail());

    int remove_callback_call_count = 0;
    int progress_callback_call_count = 0;

    std::vector<boost::filesystem::path> expected_paths = {
        m_tmp_test_dir / "a" / "b" / "c",
        m_tmp_test_dir / "a" / "b",
        m_tmp_test_dir / "a" / "e" / "f",
        m_tmp_test_dir / "a" / "e",
        m_tmp_test_dir / "a",
        m_tmp_test_dir / "h",
        m_tmp_test_dir / "."
    };

    std::set<boost::filesystem::path> actual_paths;

    io::EventLoop loop;
    io::remove_dir(loop, m_tmp_test_dir.string(), [&](const io::Status& status) {
        ++remove_callback_call_count;
        EXPECT_TRUE(status.ok());
        EXPECT_FALSE(boost::filesystem::exists(m_tmp_test_dir));
    },
    [&](const std::string& path) {
        ++progress_callback_call_count;

        boost::filesystem::path p(path);
        p.normalize();
        actual_paths.insert(p);
    });

    EXPECT_TRUE(boost::filesystem::exists(m_tmp_test_dir));

    // TODO: this will fail because task is executed before loop run
    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // EXPECT_TRUE(boost::filesystem::exists(m_tmp_test_dir));

    EXPECT_EQ(0, remove_callback_call_count);
    EXPECT_EQ(0, progress_callback_call_count);
    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(1, remove_callback_call_count);
    EXPECT_EQ(7, progress_callback_call_count);

    for (size_t i = 0; i < expected_paths.size(); ++i) {
        ASSERT_NE(actual_paths.end(), actual_paths.find(expected_paths[i])) << "i= " << i;
    }
}

TEST_F(DirTest, remove_dir_not_exist) {
    int callback_call_count = 0;
    io::EventLoop loop;

    auto remove_path = m_tmp_test_dir / "not_exist";
    io::remove_dir(loop, remove_path.string(), [&](const io::Status& status) {
        ++callback_call_count;
        EXPECT_TRUE(status.fail());
        EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, status.code());
        EXPECT_TRUE(boost::filesystem::exists(m_tmp_test_dir));
    });

    EXPECT_EQ(0, callback_call_count);
    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(1, callback_call_count);
}

TEST_F(DirTest, remove_dir_not_exist_and_progress_callback) {
    int callback_call_count = 0;
    int progress_call_count = 0;
    io::EventLoop loop;

    auto remove_path = m_tmp_test_dir / "not_exist";
    io::remove_dir(loop, remove_path.string(), [&](const io::Status& status) {
        ++callback_call_count;
        EXPECT_TRUE(status.fail());
        EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, status.code());
        EXPECT_TRUE(boost::filesystem::exists(m_tmp_test_dir));
    },
    [&](const std::string& path) {
        ++progress_call_count;
    });

    EXPECT_EQ(0, callback_call_count);
    EXPECT_EQ(0, progress_call_count);
    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(1, callback_call_count);
    EXPECT_EQ(0, progress_call_count);
}

// dir iterate not existing
