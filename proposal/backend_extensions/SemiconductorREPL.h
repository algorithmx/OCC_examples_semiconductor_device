#ifndef SEMICONDUCTOR_REPL_H
#define SEMICONDUCTOR_REPL_H

#include "../include/SemiconductorDevice.h"
#include "../include/GeometryBuilder.h" 
#include "../include/VTKExporter.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>

/**
 * @brief Command result structure for backend operations
 */
struct CommandResult {
    bool success;
    std::string message;
    nlohmann::json data;
    std::string vtkData;        // Always available for any geometry change
    std::string sessionId;
    
    CommandResult(bool s = false, const std::string& msg = "") 
        : success(s), message(msg) {}
    
    nlohmann::json toJSON() const {
        return {
            {"success", success},
            {"message", message}, 
            {"data", data},
            {"vtk_available", !vtkData.empty()},
            {"session_id", sessionId}
        };
    }
};

/**
 * @brief Geometry command structure (backend-only)
 */
struct GeometryCommand {
    std::string type;
    nlohmann::json parameters;
    std::string timestamp;
    std::string commandText;    // For REPL history
    
    GeometryCommand(const std::string& t = "", const nlohmann::json& p = {})
        : type(t), parameters(p) {
        timestamp = getCurrentTimestamp();
    }
    
private:
    std::string getCurrentTimestamp() const;
};

/**
 * @brief Backend session that manages complete device state and VTK export
 * 
 * This class maintains ALL state in the backend and provides complete
 * VTK export capabilities regardless of frontend presence.
 */
class DeviceSession {
private:
    std::unique_ptr<SemiconductorDevice> m_device;
    std::vector<GeometryCommand> m_commandHistory;
    std::map<std::string, std::string> m_snapshots;  // timestamp -> full device state
    std::string m_sessionId;
    std::string m_deviceName;
    int m_currentHistoryIndex;
    
    // Backend command processors (use your existing classes)
    CommandResult processCreateLayer(const nlohmann::json& params);
    CommandResult processRemoveLayer(const nlohmann::json& params);
    CommandResult processBooleanOperation(const nlohmann::json& params);
    CommandResult processGenerateMesh(const nlohmann::json& params);
    CommandResult processRefineMesh(const nlohmann::json& params);
    CommandResult processTransform(const nlohmann::json& params);
    
    // Backend state management
    void saveCurrentState();
    std::string generateSessionId() const;
    void updateVTKSnapshot();
    
public:
    DeviceSession(const std::string& deviceName);
    ~DeviceSession() = default;
    
    // Command execution (backend processes everything)
    CommandResult executeCommand(const std::string& commandText);  // REPL interface
    CommandResult executeJSON(const nlohmann::json& command);      // API interface
    
    // Always-available VTK export (backend-complete)
    std::string getCurrentVTK() const;                    // Latest VTK data
    std::string getCurrentVTP() const;                    // XML VTK format
    std::string getCurrentSnapshot() const;               // Full serialized state
    std::string getVisualizationData(const std::string& format) const;  // Multiple formats
    
    // State management (backend-only)
    bool restoreFromSnapshot(const std::string& timestamp);
    std::vector<std::string> getAvailableSnapshots() const;
    void createSnapshot(const std::string& name = "");
    
    // Export capabilities (backend handles all formats)
    void exportAll(const std::string& basePath) const;
    void exportVTK(const std::string& filename) const;
    void exportSTEP(const std::string& filename) const;
    void exportSTL(const std::string& filename) const;
    
    // Command history (backend-managed)
    std::vector<GeometryCommand> getCommandHistory() const { return m_commandHistory; }
    CommandResult undoLastCommand();
    CommandResult redoCommand();
    bool canUndo() const { return m_currentHistoryIndex > 0; }
    bool canRedo() const { return m_currentHistoryIndex < static_cast<int>(m_commandHistory.size()); }
    
    // Session info
    const std::string& getSessionId() const { return m_sessionId; }
    const std::string& getDeviceName() const { return m_deviceName; }
    SemiconductorDevice* getDevice() { return m_device.get(); }
    const SemiconductorDevice* getDevice() const { return m_device.get(); }
    
    // Status information
    nlohmann::json getStatus() const;
    void printStatus() const;
};

/**
 * @brief Complete REPL interface (backend-only, no frontend dependencies)
 * 
 * This provides full semiconductor device modeling capabilities via
 * command line, with complete VTK export, independent of any web frontend.
 */
class SemiconductorREPL {
private:
    std::map<std::string, std::unique_ptr<DeviceSession>> m_sessions;
    std::string m_currentSessionId;
    bool m_running;
    std::vector<std::string> m_replHistory;
    
    // Command parsing (backend text processing)
    std::vector<std::string> tokenize(const std::string& line) const;
    std::string toLowerCase(const std::string& str) const;
    void addToHistory(const std::string& line);
    
    // Built-in REPL commands (all backend operations)
    void showHelp() const;
    void showCommands() const;
    void showSessionInfo() const;
    void listAllSessions() const;
    
public:
    SemiconductorREPL();
    ~SemiconductorREPL() = default;
    
    // REPL lifecycle (completely independent)
    void start();
    void stop() { m_running = false; }
    void processLine(const std::string& line);
    void runInteractive();                    // Interactive terminal mode
    void runScript(const std::string& filename);  // Batch script mode
    
    // Session management (backend-only)
    std::string createSession(const std::string& deviceName);
    bool switchSession(const std::string& sessionId);
    bool deleteSession(const std::string& sessionId);
    std::vector<std::string> listSessions() const;
    DeviceSession* getCurrentSession();
    DeviceSession* getSession(const std::string& sessionId);
    
    // Direct API interface (for web layer)
    CommandResult executeJSON(const nlohmann::json& command, const std::string& sessionId = "");
    nlohmann::json getSessionStatus(const std::string& sessionId) const;
    std::string getVisualizationData(const std::string& sessionId, const std::string& format) const;
    
    // Command processors (using your existing classes)
    CommandResult cmd_create_device(const std::vector<std::string>& args);
    CommandResult cmd_switch_session(const std::vector<std::string>& args);
    CommandResult cmd_add_layer(const std::vector<std::string>& args);
    CommandResult cmd_remove_layer(const std::vector<std::string>& args);
    CommandResult cmd_list_layers(const std::vector<std::string>& args);
    CommandResult cmd_boolean_union(const std::vector<std::string>& args);
    CommandResult cmd_boolean_subtract(const std::vector<std::string>& args);
    CommandResult cmd_boolean_intersect(const std::vector<std::string>& args);
    CommandResult cmd_generate_mesh(const std::vector<std::string>& args);
    CommandResult cmd_refine_mesh(const std::vector<std::string>& args);
    CommandResult cmd_export_vtk(const std::vector<std::string>& args);
    CommandResult cmd_export_step(const std::vector<std::string>& args);
    CommandResult cmd_export_stl(const std::vector<std::string>& args);
    CommandResult cmd_export_all(const std::vector<std::string>& args);
    CommandResult cmd_show_status(const std::vector<std::string>& args);
    CommandResult cmd_create_mosfet(const std::vector<std::string>& args);
    CommandResult cmd_validate(const std::vector<std::string>& args);
    CommandResult cmd_undo(const std::vector<std::string>& args);
    CommandResult cmd_redo(const std::vector<std::string>& args);
    CommandResult cmd_history(const std::vector<std::string>& args);
    CommandResult cmd_snapshot(const std::vector<std::string>& args);
    CommandResult cmd_restore(const std::vector<std::string>& args);
    
private:
    // Helper methods for parsing parameters
    MaterialProperties parseMaterial(const std::string& materialName) const;
    DeviceRegion parseRegion(const std::string& regionName) const;
    Dimensions3D parseDimensions(const std::vector<std::string>& dims) const;
    gp_Pnt parsePoint(const std::vector<std::string>& coords) const;
};

/**
 * @brief Extended VTK Exporter with string outputs (backend-complete)
 * 
 * This extends your existing VTKExporter to provide in-memory string
 * outputs while maintaining all file-based export capabilities.
 */
class VTKExporterExtended : public VTKExporter {
public:
    // String-based exports (for API/web interface)
    static std::string exportToVTKString(const BoundaryMesh& mesh);
    static std::string exportToVTPString(const SemiconductorDevice& device);
    static nlohmann::json exportToWebGL(const BoundaryMesh& mesh);
    static std::string exportToSTLString(const BoundaryMesh& mesh);
    
    // Batch export methods
    static void exportDeviceAllFormats(const SemiconductorDevice& device, 
                                     const std::string& basePath);
    
    // Snapshot methods (complete device state)
    static nlohmann::json deviceToSnapshot(const SemiconductorDevice& device);
    static bool deviceFromSnapshot(SemiconductorDevice& device, 
                                  const nlohmann::json& snapshot);
    
private:
    // Helper methods for format conversion
    static nlohmann::json meshToWebGLFormat(const BoundaryMesh& mesh);
    static std::string meshToVTKString(const BoundaryMesh& mesh, const std::string& title);
    static void addMaterialColors(nlohmann::json& webglData, const SemiconductorDevice& device);
};

/**
 * @brief Example usage demonstrating backend independence
 */
class REPLExample {
public:
    static void demonstrateStandaloneUsage() {
        std::cout << "=== Standalone REPL Usage (No Frontend Required) ===\n";
        
        SemiconductorREPL repl;
        
        // All operations happen in backend
        std::string sessionId = repl.createSession("ExampleMOSFET");
        repl.switchSession(sessionId);
        
        // Execute commands (backend processes everything)
        repl.processLine("add_layer box silicon substrate 100e-6 100e-6 50e-6");
        repl.processLine("generate_mesh 1e-6");
        repl.processLine("export_vtk example.vtk");
        repl.processLine("show_status");
        
        // VTK is always available
        DeviceSession* session = repl.getCurrentSession();
        std::string vtkData = session->getCurrentVTK();
        std::cout << "VTK data length: " << vtkData.length() << " bytes\n";
        
        std::cout << "=== Backend provides complete functionality independently ===\n";
    }
};

#endif // SEMICONDUCTOR_REPL_H
