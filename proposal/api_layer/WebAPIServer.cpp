// WebAPIServer.cpp - Ultra-thin translation layer
// NO business logic - just translates between HTTP and backend REPL

#include "WebAPIServer.h"
#include "../backend_extensions/SemiconductorREPL.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

/**
 * @brief Ultra-thin API server that only translates HTTP to backend calls
 * 
 * This class contains ZERO business logic. It only:
 * 1. Receives HTTP requests
 * 2. Calls backend REPL methods
 * 3. Returns backend responses
 * 
 * ALL computation, VTK export, mesh generation, etc. happens in backend.
 */
class WebAPIServer {
private:
    httplib::Server m_server;
    std::unique_ptr<SemiconductorREPL> m_repl;  // Backend does everything
    int m_port;
    
    // Pure translation helpers (no logic)
    void sendJSON(httplib::Response& res, const nlohmann::json& data) {
        res.set_content(data.dump(), "application/json");
    }
    
    void sendError(httplib::Response& res, int code, const std::string& message) {
        res.status = code;
        nlohmann::json error = {{"error", message}};
        res.set_content(error.dump(), "application/json");
    }
    
public:
    WebAPIServer(int port = 8080) : m_port(port) {
        m_repl = std::make_unique<SemiconductorREPL>();  // Backend instance
        setupRoutes();
    }
    
    void start() {
        std::cout << "Starting API server on port " << m_port << std::endl;
        std::cout << "Backend REPL ready for commands" << std::endl;
        m_server.listen("0.0.0.0", m_port);
    }
    
    void setupRoutes() {
        // Enable CORS for web frontend
        m_server.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            return httplib::Server::HandlerResponse::Unhandled;
        });
        
        // Pure translation endpoints - NO business logic
        
        // Command execution (pure translation to backend REPL)
        m_server.Post("/api/commands", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                nlohmann::json command = nlohmann::json::parse(req.body);
                
                // Backend processes everything - API just translates
                CommandResult result = m_repl->executeJSON(command);
                
                // Just return backend result
                sendJSON(res, result.toJSON());
            } catch (const std::exception& e) {
                sendError(res, 400, "Command parsing error: " + std::string(e.what()));
            }
        });
        
        // Session management (pure translation to backend)
        m_server.Post("/api/sessions", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                nlohmann::json body = nlohmann::json::parse(req.body);
                std::string deviceName = body["device_name"];
                
                // Backend creates session - API just translates
                std::string sessionId = m_repl->createSession(deviceName);
                
                sendJSON(res, {{"session_id", sessionId}, {"device_name", deviceName}});
            } catch (const std::exception& e) {
                sendError(res, 400, "Session creation error: " + std::string(e.what()));
            }
        });
        
        // Get session status (backend provides all data)
        m_server.Get("/api/sessions/(.*)", [this](const httplib::Request& req, httplib::Response& res) {
            std::string sessionId = req.matches[1];
            
            // Backend provides complete session info
            nlohmann::json status = m_repl->getSessionStatus(sessionId);
            
            if (status.empty()) {
                sendError(res, 404, "Session not found");
            } else {
                sendJSON(res, status);
            }
        });
        
        // VTK export (backend handles complete VTK generation)
        m_server.Get("/api/sessions/(.*)/export/vtk", [this](const httplib::Request& req, httplib::Response& res) {
            std::string sessionId = req.matches[1];
            
            try {
                // Backend provides complete VTK data
                std::string vtkData = m_repl->getVisualizationData(sessionId, "vtk");
                
                if (vtkData.empty()) {
                    sendError(res, 404, "No VTK data available");
                } else {
                    // Just serve backend-generated VTK file
                    res.set_content(vtkData, "application/vtk");
                    res.set_header("Content-Disposition", "attachment; filename=\"device.vtk\"");
                }
            } catch (const std::exception& e) {
                sendError(res, 500, "VTK export error: " + std::string(e.what()));
            }
        });
        
        // WebGL visualization data (backend converts VTK to WebGL format)
        m_server.Get("/api/sessions/(.*)/visualization/webgl", [this](const httplib::Request& req, httplib::Response& res) {
            std::string sessionId = req.matches[1];
            
            try {
                // Backend converts its VTK data to WebGL format
                std::string webglData = m_repl->getVisualizationData(sessionId, "webgl");
                
                if (webglData.empty()) {
                    sendError(res, 404, "No visualization data available");
                } else {
                    res.set_content(webglData, "application/json");
                }
            } catch (const std::exception& e) {
                sendError(res, 500, "Visualization error: " + std::string(e.what()));
            }
        });
        
        // Multiple export formats (backend handles all formats)
        m_server.Get("/api/sessions/(.*)/export/(.*)", [this](const httplib::Request& req, httplib::Response& res) {
            std::string sessionId = req.matches[1];
            std::string format = req.matches[2];
            
            try {
                // Backend handles all export formats
                std::string data = m_repl->getVisualizationData(sessionId, format);
                
                if (data.empty()) {
                    sendError(res, 404, "No data available for format: " + format);
                } else {
                    // Set appropriate content type
                    std::string contentType = "application/octet-stream";
                    if (format == "vtk") contentType = "application/vtk";
                    else if (format == "stl") contentType = "application/stl";
                    else if (format == "step") contentType = "application/step";
                    else if (format == "webgl") contentType = "application/json";
                    
                    res.set_content(data, contentType);
                    res.set_header("Content-Disposition", "attachment; filename=\"device." + format + "\"");
                }
            } catch (const std::exception& e) {
                sendError(res, 500, "Export error: " + std::string(e.what()));
            }
        });
        
        // List sessions (backend manages all sessions)
        m_server.Get("/api/sessions", [this](const httplib::Request&, httplib::Response& res) {
            std::vector<std::string> sessions = m_repl->listSessions();
            sendJSON(res, {{"sessions", sessions}});
        });
        
        // Health check
        m_server.Get("/api/health", [this](const httplib::Request&, httplib::Response& res) {
            sendJSON(res, {
                {"status", "healthy"},
                {"backend", "SemiconductorREPL"},
                {"vtk_available", true},
                {"api_version", "1.0"}
            });
        });
    }
};

/**
 * @brief Example demonstrating pure translation pattern
 */
void demonstrateAPITranslation() {
    std::cout << "=== API Translation Demonstration ===\n";
    
    // Start API server (non-blocking example)
    WebAPIServer server(8080);
    
    std::cout << "API Endpoints Available:\n";
    std::cout << "  POST /api/commands           - Execute geometry commands\n";
    std::cout << "  POST /api/sessions           - Create device session\n";
    std::cout << "  GET  /api/sessions/{id}      - Get session status\n";
    std::cout << "  GET  /api/sessions/{id}/export/vtk   - Download VTK file\n";
    std::cout << "  GET  /api/sessions/{id}/export/step  - Download STEP file\n";
    std::cout << "  GET  /api/sessions/{id}/export/stl   - Download STL file\n";
    std::cout << "  GET  /api/sessions/{id}/visualization/webgl - Get WebGL data\n";
    
    std::cout << "\nAll processing happens in backend REPL.\n";
    std::cout << "API layer only translates HTTP <-> Backend calls.\n";
    
    // server.start();  // This would start the actual server
}

/**
 * @brief Example of API usage showing backend does all work
 */
void demonstrateAPIUsage() {
    std::cout << "=== API Usage Example ===\n";
    
    // This would be HTTP requests in practice
    std::cout << "1. Create device session:\n";
    std::cout << "   POST /api/sessions {\"device_name\": \"MyMOSFET\"}\n";
    std::cout << "   Response: {\"session_id\": \"abc123\", \"device_name\": \"MyMOSFET\"}\n";
    
    std::cout << "\n2. Add layer:\n";
    std::cout << "   POST /api/commands {\n";
    std::cout << "     \"session_id\": \"abc123\",\n";
    std::cout << "     \"type\": \"add_layer\",\n";
    std::cout << "     \"parameters\": {\n";
    std::cout << "       \"geometry\": \"box\",\n";
    std::cout << "       \"material\": \"silicon\",\n";
    std::cout << "       \"region\": \"substrate\",\n";
    std::cout << "       \"dimensions\": [100e-6, 100e-6, 50e-6]\n";
    std::cout << "     }\n";
    std::cout << "   }\n";
    std::cout << "   Response: {\"success\": true, \"vtk_available\": true, \"message\": \"Layer added\"}\n";
    
    std::cout << "\n3. Generate mesh:\n";
    std::cout << "   POST /api/commands {\n";
    std::cout << "     \"session_id\": \"abc123\",\n";
    std::cout << "     \"type\": \"generate_mesh\",\n";
    std::cout << "     \"parameters\": {\"mesh_size\": 1e-6}\n";
    std::cout << "   }\n";
    std::cout << "   Response: {\"success\": true, \"vtk_available\": true, \"message\": \"Mesh generated: 5231 elements\"}\n";
    
    std::cout << "\n4. Download VTK file:\n";
    std::cout << "   GET /api/sessions/abc123/export/vtk\n";
    std::cout << "   Response: Complete VTK file (generated by backend)\n";
    
    std::cout << "\n5. Get WebGL visualization:\n";
    std::cout << "   GET /api/sessions/abc123/visualization/webgl\n";
    std::cout << "   Response: {\"vertices\": [...], \"indices\": [...], \"materials\": [...]}\n";
    
    std::cout << "\n=== Backend handles ALL computation, API just translates ===\n";
}

int main() {
    std::cout << "=== WebAPI Server - Pure Translation Layer ===\n";
    
    demonstrateAPITranslation();
    demonstrateAPIUsage();
    
    std::cout << "\nTo start server: WebAPIServer server(8080); server.start();\n";
    
    return 0;
}
