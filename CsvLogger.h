#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace csvlog
{

struct CsvLoggerConfig
{
    std::string filePath;
    std::string headerText;
    int intervalSec{1};
    bool append{true};
};

enum class CsvItemKind
{
    Auto,
    Manual
};

class ICsvItem
{
public:
    virtual ~ICsvItem() = default;

    virtual std::string title() const = 0;
    virtual std::string valueAsString() const = 0;
    virtual void clear() {};
};

struct CsvItemEntry
{
    ICsvItem* item{nullptr};
    CsvItemKind kind{CsvItemKind::Auto};

    CsvItemEntry(ICsvItem* i, CsvItemKind k)
        : item(i), kind(k)
    {}
};

class CsvStringItem final : public ICsvItem
{
public:
    explicit CsvStringItem(std::string title);

    void setValue(const std::string& value);

    std::string title() const override;
    std::string valueAsString() const override;
    void clear() override;

private:
    std::string m_title;
    std::string m_value;
    mutable std::mutex m_mutex;
};

class CsvAtomicIntItem final : public ICsvItem
{
public:
    explicit CsvAtomicIntItem(std::string title, int initialValue = 0);

    void setValue(int value);

    std::string title() const override;
    std::string valueAsString() const override;

private:
    std::string m_title;
    std::atomic<int> m_value;
};

class CsvFileWriter
{
public:
    CsvFileWriter() = default;
    ~CsvFileWriter();

    CsvFileWriter(const CsvFileWriter&) = delete;
    CsvFileWriter& operator=(const CsvFileWriter&) = delete;

    bool open(const std::string& filePath, bool append);
    void close();
    bool isOpen() const;

    bool writeHeader(const std::string& headerText,
                     const std::vector<std::string>& columns);
    bool writeRow(const std::vector<std::string>& values);

private:
    static std::string escape(const std::string& value);
    static std::string joinCsv(const std::vector<std::string>& values);
    static bool fileExistsAndNotEmpty(const std::string& filePath);

private:
    std::ofstream m_stream;
    bool m_headerWritten{false};
};

class CsvLogger
{
public:
    explicit CsvLogger(CsvLoggerConfig config);
    ~CsvLogger();

    CsvLogger(const CsvLogger&) = delete;
    CsvLogger& operator=(const CsvLogger&) = delete;

    bool setConfig(const CsvLoggerConfig& config);
    bool addItem(ICsvItem* item, CsvItemKind kind);
    bool clearItems();

    bool start();
    void stop();
    bool isRunning() const;

    void write();

    void pause();
    void resume();
    bool isPaused() const;

private:
    void run();
    std::vector<std::string> buildHeaderColumns();
    std::vector<std::string> buildRowValues(const std::string& recordType);
    static std::string readProcUptime();
    static std::string makeSystemTimeString();

    bool writeRow(const std::string& recordType);
    void clearManualItems();

private:
    CsvLoggerConfig m_config;
    std::vector<CsvItemEntry> m_items;
    CsvFileWriter m_writer;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_autoWritePaused{false};

    mutable std::mutex m_itemMutex;
    std::mutex m_waitMutex;
    std::condition_variable m_waitCv;
};

} // namespace csvlog
