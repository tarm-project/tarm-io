/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/


#include "UTCommon.h"

#include "fs/Dir.h"

#include <boost/filesystem.hpp>

#include <vector>
#include <thread>

#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    #include <sys/stat.h>
#endif

struct DirTest : public testing::Test,
                 public LogRedirector {
    DirTest() {
        m_tmp_test_dir = create_temp_test_directory();
    }

protected:
    boost::filesystem::path m_tmp_test_dir;
};

TEST_F(DirTest, default_state) {
    io::EventLoop loop;

    auto dir = new io::fs::Dir(loop);
    EXPECT_TRUE(dir->path().empty());
    EXPECT_FALSE(dir->is_open());

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    dir->schedule_removal();
}

TEST_F(DirTest, directory_entry_type_to_ostream) {
    // Just basic check that it works
    std::stringstream ss;
    ss << io::fs::DirectoryEntryType::FILE;
    EXPECT_EQ("FILE", ss.str());
}

TEST_F(DirTest, open_then_close) {
    io::EventLoop loop;
    auto dir = new io::fs::Dir(loop);

    dir->open(m_tmp_test_dir.string(), [&](io::fs::Dir& dir, const io::Error& error) {
        EXPECT_FALSE(error);
        EXPECT_EQ(io::StatusCode::OK, error.code());

        EXPECT_EQ(m_tmp_test_dir, dir.path().string());
        EXPECT_TRUE(dir.is_open());
        dir.close();
        // basic properties are not changed immediately, but only after async close execution.
        // In callback which is not present here
        EXPECT_EQ(m_tmp_test_dir, dir.path().string());
        EXPECT_TRUE(dir.is_open());
    });
    EXPECT_FALSE(dir->is_open());

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    dir->schedule_removal();
}

TEST_F(DirTest, open_not_existing) {
    io::EventLoop loop;
    auto dir = new io::fs::Dir(loop);

    std::size_t on_open_call_count = 0;

    auto path = (m_tmp_test_dir / "not_exists").string();
    dir->open(path, [&](io::fs::Dir& dir, const io::Error& error) {
        EXPECT_TRUE(error);
        EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, error.code());

        ++on_open_call_count;

        EXPECT_EQ(path, dir.path());
        EXPECT_FALSE(dir.is_open());
    });

    EXPECT_EQ(0, on_open_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_open_call_count);

    EXPECT_EQ("", dir->path());

    ASSERT_EQ(io::StatusCode::OK, loop.run());
    dir->schedule_removal();
}

TEST_F(DirTest, open_close_open) {
    io::EventLoop loop;
    auto dir = new io::fs::Dir(loop);

    std::size_t on_open_count = 0;

    const io::fs::Path dir_1 = io::fs::Path(m_tmp_test_dir.string()) / "dir_1";
    const io::fs::Path dir_2 = io::fs::Path(m_tmp_test_dir.string()) / "dir_2";
    boost::filesystem::create_directories(dir_1.string());
    boost::filesystem::create_directories(dir_2.string());

    dir->open(dir_1, [&](io::fs::Dir& dir, const io::Error& error) {
        EXPECT_FALSE(error) << error;
        ++on_open_count;

        dir.close([&](io::fs::Dir& dir, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            dir.open(dir_2, [&](io::fs::Dir& dir, const io::Error& error) {
                EXPECT_FALSE(error) << error;
                ++on_open_count;
                EXPECT_EQ(dir_2.string(), dir.path().string());
                dir.schedule_removal();
            });
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

// TODO: rewrite this test for Dir
/*
TEST_F(FileTest, double_close_parallel) {
     auto path = create_empty_file(m_tmp_test_dir);
     ASSERT_FALSE(path.empty());

     std::size_t close_count = 0;

     io::EventLoop loop;

     auto file = new io::fs::File(loop);
     file->open(path, [&](io::fs::File& file, const io::Error& error) {
         EXPECT_TRUE(file.is_open());

         file.close([&](io::fs::File& file, const io::Error& error) {
             EXPECT_FALSE(error) << error;
             ++close_count;
         });
         file.close([&](io::fs::File& file, const io::Error& error) {
             EXPECT_TRUE(error);
             EXPECT_EQ(io::StatusCode::OPERATION_ALREADY_IN_PROGRESS, error.code());
             ++close_count;
         });
     });

     EXPECT_EQ(0, close_count);

     ASSERT_EQ(io::StatusCode::OK, loop.run());
     file->schedule_removal();

     EXPECT_EQ(2, close_count);

     ASSERT_EQ(io::StatusCode::OK, loop.run());
 }
 */

TEST_F(DirTest, list_not_opened) {
    std::size_t list_call_count = 0;
    std::size_t end_list_call_count = 0;

    io::EventLoop loop;
    auto dir = new io::fs::Dir(loop);
    dir->list(
        [&](io::fs::Dir& dir, const char* name, io::fs::DirectoryEntryType entry_type) {
            ++list_call_count;
        },
        [&](io::fs::Dir& dir, const io::Error& error) {
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::DIR_NOT_OPEN, error.code());
            ++end_list_call_count;
            dir.schedule_removal();
        }
    );

    EXPECT_EQ(0, list_call_count);
    EXPECT_EQ(0, end_list_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, list_call_count);
    EXPECT_EQ(1, end_list_call_count);
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
    auto dir = new io::fs::Dir(loop);
    dir->open(m_tmp_test_dir.string(), [&](io::fs::Dir& dir, const io::Error&) {
        dir.list([&](io::fs::Dir& dir, const char* name, io::fs::DirectoryEntryType entry_type) {
            if (std::string(name) == "dir_1") {
                EXPECT_FALSE(dir_1_listed);
                EXPECT_EQ(io::fs::DirectoryEntryType::DIR, entry_type);
                dir_1_listed = true;
            } else if (std::string(name) == "dir_2") {
                EXPECT_FALSE(dir_2_listed);
                EXPECT_EQ(io::fs::DirectoryEntryType::DIR, entry_type);
                dir_2_listed = true;
            } else if (std::string(name) == "dir_3") {
                EXPECT_FALSE(dir_3_listed);
                EXPECT_EQ(io::fs::DirectoryEntryType::DIR, entry_type);
                dir_3_listed = true;
            } else if (std::string(name) == "file_1") {
                EXPECT_FALSE(file_1_listed);
                EXPECT_EQ(io::fs::DirectoryEntryType::FILE, entry_type);
                file_1_listed = true;
            } else if (std::string(name) == "file_2") {
                EXPECT_FALSE(file_2_listed);
                EXPECT_EQ(io::fs::DirectoryEntryType::FILE, entry_type);
                file_2_listed = true;
            }
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_TRUE(dir_1_listed);
    EXPECT_TRUE(dir_2_listed);
    EXPECT_TRUE(dir_3_listed);
    EXPECT_TRUE(file_1_listed);
    EXPECT_TRUE(file_2_listed);

    dir->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(DirTest, close_in_list_callback) {
    // TODO:
}

TEST_F(DirTest, empty_dir) {
    io::EventLoop loop;
    auto dir = new io::fs::Dir(loop);

    std::size_t list_call_count = 0;
    std::size_t end_list_call_count = 0;

    dir->open(m_tmp_test_dir.string(), [&](io::fs::Dir& dir, const io::Error& error) {
        EXPECT_FALSE(error) << error;
        dir.list([&](io::fs::Dir& dir, const char* name, io::fs::DirectoryEntryType entry_type) {
            ++list_call_count;
        },
        [&](io::fs::Dir& dir, const io::Error& error) {
            EXPECT_FALSE(error) << error;
            ++end_list_call_count;
        });
    });

    EXPECT_EQ(0, list_call_count);
    EXPECT_EQ(0, end_list_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(0, list_call_count);
    EXPECT_EQ(1, end_list_call_count);

    dir->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(DirTest, no_list_callback) {
    {
        std::ofstream ofile((m_tmp_test_dir/ "some_file").string());
        ASSERT_FALSE(ofile.fail());
    }

    std::size_t end_list_call_count = 0;

    io::EventLoop loop;
    auto dir = new io::fs::Dir(loop);

    dir->open(m_tmp_test_dir.string(), [&](io::fs::Dir& dir, const io::Error& error) {
        EXPECT_FALSE(error) << error;
        dir.list(
            nullptr,
            [&](io::fs::Dir& dir, const io::Error& error) {
                EXPECT_FALSE(error) << error;
                ++end_list_call_count;
            }
        );
    });

    EXPECT_EQ(0, end_list_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, end_list_call_count);

    dir->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
}

TEST_F(DirTest, list_symlink) {
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    auto file_path = m_tmp_test_dir / "some_file";
    {
        std::ofstream ofile(file_path.string());
        ASSERT_FALSE(ofile.fail());
    }

    ASSERT_NO_THROW(boost::filesystem::create_symlink(file_path, m_tmp_test_dir / "link"));

    std::size_t entries_count = 0;
    bool link_found = false;

    io::EventLoop loop;
    auto dir = new io::fs::Dir(loop);
    dir->open(m_tmp_test_dir.string(), [&](io::fs::Dir& dir, const io::Error&) {
        dir.list([&](io::fs::Dir& dir, const char* name, io::fs::DirectoryEntryType entry_type) {
            if (std::string(name) == "some_file") {
                EXPECT_EQ(io::fs::DirectoryEntryType::FILE, entry_type);
            } else if (std::string(name) == "link") {
                EXPECT_EQ(io::fs::DirectoryEntryType::LINK, entry_type);
                link_found = true;
            }

            ++entries_count;
        });
    });

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_TRUE(link_found);
    EXPECT_EQ(2, entries_count);

    dir->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
#else
    TARM_IO_TEST_SKIP();
#endif
}

TEST_F(DirTest, list_block_and_char_devices) {
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    std::size_t block_devices_count = 0;
    std::size_t char_devices_count = 0;

    io::EventLoop loop;
    auto dir = new io::fs::Dir(loop);
    dir->open("/dev", [&](io::fs::Dir& dir, const io::Error& error) {
        EXPECT_TRUE(!error);

        dir.list([&](io::fs::Dir& dir, const char* name, io::fs::DirectoryEntryType entry_type) {
            if (entry_type == io::fs::DirectoryEntryType::BLOCK) {
                ++block_devices_count;
            } else if (entry_type == io::fs::DirectoryEntryType::CHAR) {
                ++char_devices_count;
            }
        });
    });

    EXPECT_EQ(0, block_devices_count);
    EXPECT_EQ(0, char_devices_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_GT(block_devices_count, 0);
    EXPECT_GT(char_devices_count, 0);

    dir->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
#else
    TARM_IO_TEST_SKIP();
#endif
}

TEST_F(DirTest, list_domain_sockets) {
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    std::size_t domain_sockets_count = 0;

    io::EventLoop loop;
    auto dir = new io::fs::Dir(loop);
    dir->open("/var/run", [&](io::fs::Dir& dir, const io::Error& error) {
        EXPECT_TRUE(!error);

        dir.list([&](io::fs::Dir& dir, const char* name, io::fs::DirectoryEntryType entry_type) {
            if (entry_type == io::fs::DirectoryEntryType::SOCKET) {
                ++domain_sockets_count;
            }
        });
    });

    EXPECT_EQ(0, domain_sockets_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_GT(domain_sockets_count, 0);

    dir->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
#else
    TARM_IO_TEST_SKIP();
#endif
}

TEST_F(DirTest, list_fifo) {
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    std::size_t fifo_count = 0;

    auto fifo_status = mkfifo((m_tmp_test_dir / "fifo").string().c_str(), S_IRWXU);
    ASSERT_EQ(0, fifo_status);

    io::EventLoop loop;
    auto dir = new io::fs::Dir(loop);
    dir->open(m_tmp_test_dir.string(), [&](io::fs::Dir& dir, const io::Error& error) {
        EXPECT_FALSE(error) << error;

        dir.list([&](io::fs::Dir& dir, const char* name, io::fs::DirectoryEntryType entry_type) {
            if (entry_type == io::fs::DirectoryEntryType::FIFO) {
                ++fifo_count;
            }
        });
    });

    EXPECT_EQ(0, fifo_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, fifo_count);

    dir->schedule_removal();
    ASSERT_EQ(io::StatusCode::OK, loop.run());
#else
    TARM_IO_TEST_SKIP();
#endif
}

TEST_F(DirTest, make_temp_dir) {
    io::EventLoop loop;

    std::size_t on_temp_dir_call_count = 0;

    const std::string template_path = (m_tmp_test_dir / "temp-XXXXXX").string();

    io::fs::make_temp_dir(loop, template_path,
        [&](const std::string& dir, const io::Error& error) {
            ++on_temp_dir_call_count;
            EXPECT_FALSE(error);
            EXPECT_EQ(template_path.size(), dir.size());
            EXPECT_EQ(0, dir.find((m_tmp_test_dir / "temp-").string()));
            EXPECT_TRUE(boost::filesystem::exists(dir));
        }
    );

    EXPECT_EQ(0, on_temp_dir_call_count); // Checking that operation is really async

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_temp_dir_call_count);
}

// TOOD: this test is passing on OS X. Need to hide pattern inside the call
TEST_F(DirTest, DISABLED_make_temp_dir_invalid_template) {
    io::EventLoop loop;

    std::size_t on_temp_dir_call_count = 0;

    // There should be 6 'X' chars
    const std::string template_path = (m_tmp_test_dir / "temp-XXXXX").string();

    io::fs::make_temp_dir(loop, template_path,
        [&](const std::string& dir, const io::Error& error) {
            ++on_temp_dir_call_count;
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
        }
    );

    EXPECT_EQ(0, on_temp_dir_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_temp_dir_call_count);
}

TEST_F(DirTest, make_dir) {
    io::EventLoop loop;

    std::size_t on_make_dir_call_count = 0;

    const std::string path = (m_tmp_test_dir / "new_dir").string();

    io::fs::make_dir(loop, path,
        [&](const io::Error& error) {
            ++on_make_dir_call_count;
            EXPECT_FALSE(error);
            EXPECT_TRUE(boost::filesystem::exists(path));
        }
    );

    EXPECT_EQ(0, on_make_dir_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_make_dir_call_count);
}

TEST_F(DirTest, make_dir_no_such_dir_error) {
    io::EventLoop loop;

    std::size_t on_make_dir_call_count = 0;

    const std::string path = (m_tmp_test_dir / "no_exists" / "new_dir").string();

    io::fs::make_dir(loop, path,
        [&](const io::Error& error) {
            ++on_make_dir_call_count;
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, error.code());
        }
    );

    EXPECT_EQ(0, on_make_dir_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_make_dir_call_count);
}

TEST_F(DirTest, make_dir_exists_error) {
    io::EventLoop loop;

    std::size_t on_make_dir_call_count = 0;

    const std::string path = (m_tmp_test_dir).string();

    io::fs::make_dir(loop, path,
        [&](const io::Error& error) {
            ++on_make_dir_call_count;
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::FILE_OR_DIR_ALREADY_EXISTS, error.code());
        }
    );

    EXPECT_EQ(0, on_make_dir_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_make_dir_call_count);
}

TEST_F(DirTest, make_dir_empty_path_error) {
    io::EventLoop loop;

    std::size_t on_make_dir_call_count = 0;

    io::fs::make_dir(loop, "",
        [&](const io::Error& error) {
            ++on_make_dir_call_count;
            EXPECT_TRUE(error);
            // TODO:
            // On Linux io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY is returned here
            //EXPECT_EQ(io::StatusCode::INVALID_ARGUMENT, error.code());
        }
    );

    EXPECT_EQ(0, on_make_dir_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_make_dir_call_count);
}

TEST_F(DirTest, make_dir_name_to_long_error) {
    io::EventLoop loop;

    std::size_t on_make_dir_call_count = 0;

    const std::string path = (m_tmp_test_dir / "1234567890qwertyuiopasdfgghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm1234567890qwertyuiopasdfghjklzxcvbnm").string();

    io::fs::make_dir(loop, path,
        [&](const io::Error& error) {
            ++on_make_dir_call_count;
            EXPECT_TRUE(error);
            EXPECT_EQ(io::StatusCode::NAME_TOO_LONG, error.code());
        }
    );

    EXPECT_EQ(0, on_make_dir_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_make_dir_call_count);
}

TEST_F(DirTest, make_dir_root_dir_error) {
#if defined(TARM_IO_PLATFORM_MACOSX) || defined(TARM_IO_PLATFORM_LINUX)
    io::EventLoop loop;

    std::size_t on_make_dir_call_count = 0;

    io::fs::make_dir(loop, "/",
        [&](const io::Error& error) {
            ++on_make_dir_call_count;
            EXPECT_TRUE(error);
            // TODO:
            // On Linux another error: io::StatusCode::FILE_OR_DIR_ALREADY_EXISTS
            //EXPECT_EQ(io::StatusCode::ILLEGAL_OPERATION_ON_A_DIRECTORY, error.code());
        }
    );

    EXPECT_EQ(0, on_make_dir_call_count);

    ASSERT_EQ(io::StatusCode::OK, loop.run());

    EXPECT_EQ(1, on_make_dir_call_count);
#else
    TARM_IO_TEST_SKIP();
#endif
}


TEST_F(DirTest, remove_dir) {
    // If directory creation fail, exception will be thrown
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

    io::fs::remove_dir(loop, m_tmp_test_dir.string(), [&](const io::Error& error) {
        ++callback_call_count;
        EXPECT_FALSE(error);
        EXPECT_FALSE(boost::filesystem::exists(m_tmp_test_dir));
    });

    EXPECT_EQ(0, callback_call_count);
    ASSERT_EQ(io::StatusCode::OK, loop.run());
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
        m_tmp_test_dir
    };

    std::set<boost::filesystem::path> actual_paths;

    io::EventLoop loop;
    io::fs::remove_dir(loop, m_tmp_test_dir.string(), [&](const io::Error& error) {
        ++remove_callback_call_count;
        EXPECT_FALSE(error);
        EXPECT_FALSE(boost::filesystem::exists(m_tmp_test_dir));
    },
    [&](const std::string& path) {
        ++progress_callback_call_count;

        boost::filesystem::path p(path);
        p.normalize();
        actual_paths.insert(p);
    });

    EXPECT_TRUE(boost::filesystem::exists(m_tmp_test_dir));

    // Guarantee that code is not executed before loop run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(boost::filesystem::exists(m_tmp_test_dir));

    EXPECT_EQ(0, remove_callback_call_count);
    EXPECT_EQ(0, progress_callback_call_count);
    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(1, remove_callback_call_count);
    EXPECT_EQ(7, progress_callback_call_count);

    for (size_t i = 0; i < expected_paths.size(); ++i) {
        ASSERT_NE(actual_paths.end(), actual_paths.find(expected_paths[i])) << "i= " << i << " " << expected_paths[i];
    }
}

TEST_F(DirTest, remove_dir_not_exist) {
    int callback_call_count = 0;
    io::EventLoop loop;

    auto remove_path = m_tmp_test_dir / "not_exist";
    io::fs::remove_dir(loop, remove_path.string(), [&](const io::Error& error) {
        ++callback_call_count;
        EXPECT_TRUE(error);
        EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, error.code());
        EXPECT_TRUE(boost::filesystem::exists(m_tmp_test_dir));
    });

    EXPECT_EQ(0, callback_call_count);
    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(1, callback_call_count);
}

TEST_F(DirTest, remove_dir_not_exist_and_progress_callback) {
    int callback_call_count = 0;
    int progress_call_count = 0;
    io::EventLoop loop;

    auto remove_path = m_tmp_test_dir / "not_exist";
    io::fs::remove_dir(loop, remove_path.string(), [&](const io::Error& error) {
        ++callback_call_count;
        EXPECT_TRUE(error);
        EXPECT_EQ(io::StatusCode::NO_SUCH_FILE_OR_DIRECTORY, error.code());
        EXPECT_TRUE(boost::filesystem::exists(m_tmp_test_dir));
    },
    [&](const std::string& path) {
        ++progress_call_count;
    });

    EXPECT_EQ(0, callback_call_count);
    EXPECT_EQ(0, progress_call_count);
    ASSERT_EQ(io::StatusCode::OK, loop.run());
    EXPECT_EQ(1, callback_call_count);
    EXPECT_EQ(0, progress_call_count);
}

// TODO: dir iterate not existing
// TODO: open dir which is file
// TODO: directory creation permissions
// TODO: check for error if creating without write permission in path components
