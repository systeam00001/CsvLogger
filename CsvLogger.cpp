#include "CsvLogger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <ios>
#include <sstream>
#include <utility>

namespace csvlog
{

CsvStringItem::CsvStringItem(std::string title)
    : m_title(std::move(title))
{
}

void CsvStringItem::setValue(const std::string& value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_value = value;
}

std::string CsvStringItem::title() const
{
    return m_title;
}

std::string CsvStringItem::valueAsString() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_value;
}

void CsvStringItem::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_value.clear();
}

CsvAtomicIntItem::CsvAtomicIntItem(std::string title, int initialValue)
    : m_title(std::move(title))
    , m_value(initialValue)
{
}

void CsvAtomicIntItem::setValue(int value)
{
    m_value.store(value, std::memory_order_relaxed);
}

std::string CsvAtomicIntItem::title() const
{
    return m_title;
}

std::string CsvAtomicIntItem::valueAsString() const
{
    return std::to_string(m_value.load(std::memory_order_relaxed));
}

CsvFileWriter::~CsvFileWriter()
{
    close();
}

bool CsvFileWriter::open(const std::string& filePath, bool append)
{
    close();

    const bool hasExistingContent = fileExistsAndNotEmpty(filePath);

    std::ios::openmode mode = std::ios::out;
    mode |= append ? std::ios::app : std::ios::trunc;

    m_stream.open(filePath, mode);
    if (!m_stream.is_open())
    {
        return false;
    }

    m_headerWritten = append && hasExistingContent;
    return true;
}

void CsvFileWriter::close()
{
    if (m_stream.is_open())
    {
        m_stream.flush();
        m_stream.close();
    }
    m_headerWritten = false;
}

bool CsvFileWriter::isOpen() const
{
    return m_stream.is_open();
}

bool CsvFileWriter::writeHeader(const std::string& headerText,
                                const std::vector<std::string>& columns)
{
    if (!m_stream.is_open())
    {
        return false;
    }

    if (m_headerWritten)
    {
        return true;
    }

    if (!headerText.empty())
    {
        std::istringstream iss(headerText);
        std::string line;
        while (std::getline(iss, line))
        {
            m_stream << "# " << line << '\n';
        }
    }

    m_stream << joinCsv(columns) << '\n';
    m_stream.flush();

    if (!m_stream.good())
    {
        return false;
    }

    m_headerWritten = true;
    return true;
}

bool CsvFileWriter::writeRow(const std::vector<std::string>& values)
{
    if (!m_stream.is_open())
    {
        return false;
    }

    m_stream << joinCsv(values) << '\n';
    m_stream.flush();
    return m_stream.good();
}

std::string CsvFileWriter::escape(const std::string& value)
{
    bool needsQuote = false;
    std::string escaped;
    escaped.reserve(value.size() + 8);

    for (char ch : value)
    {
        if (ch == '"' || ch == ',' || ch == '\n' || ch == '\r')
        {
            needsQuote = true;
        }

        if (ch == '"')
        {
            escaped += "\"\"";
        }
        else
        {
            escaped += ch;
        }
    }

    if (!needsQuote)
    {
        return escaped;
    }

    return "\"" + escaped + "\"";
}

std::string CsvFileWriter::joinCsv(const std::vector<std::string>& values)
{
    std::ostringstream oss;

    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
        {
            oss << ',';
        }
        oss << escape(values[i]);
    }

    return oss.str();
}

bool CsvFileWriter::fileExistsAndNotEmpty(const std::string& filePath)
{
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open())
    {
        return false;
    }

    ifs.seekg(0, std::ios::end);
    return ifs.tellg() > 0;
}

CsvLogger::CsvLogger(CsvLoggerConfig config)
    : m_config(std::move(config))
{
}

CsvLogger::~CsvLogger()
{
    stop();
}

bool CsvLogger::setConfig(const CsvLoggerConfig& config)
{
    if (isRunning())
    {
        return false;
    }

    m_config = config;
    return true;
}

bool CsvLogger::addItem(ICsvItem* item, CsvItemKind kind)
{
    if (item == nullptr || isRunning())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_itemMutex);
    m_items.emplace_back(CsvItemEntry{item, kind});

    return true;
}

bool CsvLogger::clearItems()
{
    if (isRunning())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_itemMutex);
    m_items.clear();
    return true;
}

bool CsvLogger::start()
{
    if (isRunning())
    {
        return false;
    }

    if (m_config.filePath.empty() || m_config.intervalSec <= 0)
    {
        return false;
    }

    if (!m_writer.open(m_config.filePath, m_config.append))
    {
        return false;
    }

    if (!m_writer.writeHeader(m_config.headerText, buildHeaderColumns()))
    {
        m_writer.close();
        return false;
    }

    m_stopRequested.store(false, std::memory_order_release);
    m_running.store(true, std::memory_order_release);
    m_thread = std::thread(&CsvLogger::run, this);
    return true;
}

void CsvLogger::stop()
{
    if (!isRunning())
    {
        return;
    }

    m_stopRequested.store(true, std::memory_order_release);
    m_waitCv.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    m_writer.close();
    m_running.store(false, std::memory_order_release);
}

bool CsvLogger::isRunning() const
{
    return m_running.load(std::memory_order_acquire);
}

void CsvLogger::run()
{
    while (!m_stopRequested.load(std::memory_order_acquire))
    {
        if (!isPaused())
        {
            const bool ret = writeRow("AUTO");
            if (!ret)
            {
                // TODO: error handling policy later
            }
        }
        
        std::unique_lock<std::mutex> lock(m_waitMutex);
        m_waitCv.wait_for(lock,
                          std::chrono::seconds(m_config.intervalSec),
                          [this]()
                          {
                              return m_stopRequested.load(std::memory_order_acquire);
                          });
    }

    m_running.store(false, std::memory_order_release);
}

std::vector<std::string> CsvLogger::buildHeaderColumns()
{
    std::vector<std::string> columns;
    columns.emplace_back("uptime");
    columns.emplace_back("system_time");
    columns.emplace_back("record_type");

    std::lock_guard<std::mutex> lock(m_itemMutex);
    columns.reserve(columns.size() + m_items.size());

    for (const auto &entry : m_items)
    {
        if (entry.item != nullptr)
        {
            columns.push_back(entry.item->title());
        }
    }

    return columns;
}

std::vector<std::string> CsvLogger::buildRowValues(const std::string& recordType)
{
    std::vector<std::string> values;
    values.emplace_back(readProcUptime());
    values.emplace_back(makeSystemTimeString());
    values.emplace_back(recordType);

    std::lock_guard<std::mutex> lock(m_itemMutex);
    values.reserve(values.size() + m_items.size());

    for (const auto& entry : m_items)
    {
        if (nullptr == entry.item)
        {
            values.emplace_back("");
            continue;
        }

        if (entry.kind == CsvItemKind::Auto)
        {
            values.push_back(entry.item->valueAsString());
        }
        else
        {
            if (recordType == "MANUAL")
            {
                values.push_back(entry.item->valueAsString());
            }
            else
            {
                values.emplace_back("");
            }
        }
    }

    return values;
}

std::string CsvLogger::readProcUptime()
{
    std::ifstream ifs("/proc/uptime");
    if (!ifs.is_open())
    {
        return {};
    }

    double uptimeSeconds = 0.0;
    ifs >> uptimeSeconds;

    if (!ifs.good() && ifs.fail())
    {
        return {};
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << uptimeSeconds;
    return oss.str();
}

std::string CsvLogger::makeSystemTimeString()
{
    const std::time_t now = std::time(nullptr);
    std::tm tmValue{};

#if defined(_WIN32)
    localtime_s(&tmValue, &now);
#else
    localtime_r(&now, &tmValue);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tmValue, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool CsvLogger::writeRow(const std::string& recordType)
{
    const std::vector<std::string> row = buildRowValues(recordType);
    const bool ret = m_writer.writeRow(row);

    if (ret && recordType == "MANUAL")
    {
        clearManualItems();
    }

    return ret;
}

void CsvLogger::write()
{
    writeRow("MANUAL");
}

void CsvLogger::pause()
{
    m_autoWritePaused.store(true, std::memory_order_release);
}

void CsvLogger::resume()
{
    m_autoWritePaused.store(false, std::memory_order_release);
}

bool CsvLogger::isPaused() const
{
    return m_autoWritePaused.load(std::memory_order_acquire);
}

void CsvLogger::clearManualItems()
{
    std::lock_guard<std::mutex> lock(m_itemMutex);

    for (const auto& entry : m_items)
    {        
        if (entry.kind != CsvItemKind::Manual || entry.item == nullptr)
        {
            continue;
        }

        entry.item->clear();  
    }
}

} // namespace csvlog
