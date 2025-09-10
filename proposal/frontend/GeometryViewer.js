/**
 * Frontend GeometryViewer that displays geometry from your existing VTK backend
 * 
 * This class consumes data from your existing VTKExporter via the web API,
 * providing real-time 3D visualization without changing your C++ backend.
 */
class GeometryViewer {
    constructor(containerId) {
        this.containerId = containerId;
        this.apiBaseUrl = 'http://localhost:8080/api';
        this.currentDeviceId = null;
        
        // Three.js setup
        this.scene = new THREE.Scene();
        this.camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
        
        // Geometry state
        this.meshObjects = new Map(); // faceId -> Three.js mesh
        this.materials = new Map();   // materialId -> Three.js material
        
        // WebSocket for real-time updates
        this.socket = null;
        
        this.setupRenderer();
        this.setupLighting();
        this.setupCamera();
    }
    
    setupRenderer() {
        const container = document.getElementById(this.containerId);
        this.renderer.setSize(container.clientWidth, container.clientHeight);
        this.renderer.setClearColor(0x222222);
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        container.appendChild(this.renderer.domElement);
        
        // Handle window resize
        window.addEventListener('resize', () => this.onWindowResize());
        
        // Start render loop
        this.animate();
    }
    
    setupLighting() {
        // Ambient light
        const ambientLight = new THREE.AmbientLight(0x404040, 0.4);
        this.scene.add(ambientLight);
        
        // Directional light
        const directionalLight = new THREE.DirectionalLight(0xffffff, 0.6);
        directionalLight.position.set(10, 10, 5);
        directionalLight.castShadow = true;
        this.scene.add(directionalLight);
        
        // Point light
        const pointLight = new THREE.PointLight(0xffffff, 0.3);
        pointLight.position.set(-10, 10, -5);
        this.scene.add(pointLight);
    }
    
    setupCamera() {
        this.camera.position.set(5, 5, 5);
        this.camera.lookAt(0, 0, 0);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.25;
        this.controls.enableZoom = true;
    }
    
    /**
     * Load device geometry from your existing backend via web API
     */
    async loadDevice(deviceId) {
        try {
            this.currentDeviceId = deviceId;
            
            // Get device info using your existing SemiconductorDevice data
            const deviceResponse = await fetch(`${this.apiBaseUrl}/devices/${deviceId}`);
            const deviceData = await deviceResponse.json();
            
            console.log('Loading device:', deviceData.name, 'with', deviceData.layerCount, 'layers');
            
            // Get Three.js mesh data converted from your VTKExporter output
            const meshResponse = await fetch(`${this.apiBaseUrl}/devices/${deviceId}/visualization/threejs`);
            const meshData = await meshResponse.json();
            
            // Clear existing geometry
            this.clearGeometry();
            
            // Load mesh data
            this.loadMeshData(meshData);
            
            // Setup real-time updates
            this.setupWebSocket(deviceId);
            
            // Fit camera to geometry
            this.fitCameraToGeometry();
            
        } catch (error) {
            console.error('Error loading device:', error);
        }
    }
    
    /**
     * Load mesh data that comes from your existing VTKExporter
     * The web API converts your VTK data to Three.js-compatible format
     */
    loadMeshData(meshData) {
        // Create materials for different regions/materials
        this.createMaterials(meshData.materials);
        
        // Create geometry for each layer/face
        meshData.layers.forEach((layerData, layerIndex) => {
            this.createLayerMesh(layerData, layerIndex);
        });
        
        console.log(`Loaded ${meshData.layers.length} layers with ${meshData.totalElements} total elements`);
    }
    
    createMaterials(materialData) {
        const materialColors = {
            'Silicon': 0x808080,
            'Silicon_Dioxide': 0x87CEEB,
            'Metal_Contact': 0xFFD700,
            'GalliumArsenide': 0x8B4513,
            'Silicon_Nitride': 0x4B0082
        };
        
        materialData.forEach(material => {
            const color = materialColors[material.name] || 0x888888;
            const threeMaterial = new THREE.MeshPhongMaterial({
                color: color,
                transparent: true,
                opacity: 0.8,
                side: THREE.DoubleSide
            });
            
            this.materials.set(material.id, threeMaterial);
        });
    }
    
    createLayerMesh(layerData, layerIndex) {
        // Create geometry from triangular mesh data (from your BoundaryMesh)
        const geometry = new THREE.BufferGeometry();
        
        // Vertices (from your mesh nodes)
        const vertices = new Float32Array(layerData.vertices);
        geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
        
        // Indices (from your mesh elements)
        const indices = new Uint32Array(layerData.indices);
        geometry.setIndex(new THREE.BufferAttribute(indices, 1));
        
        // Normals
        geometry.computeVertexNormals();
        
        // Get material for this layer
        const material = this.materials.get(layerData.materialId) || 
                        new THREE.MeshPhongMaterial({ color: 0x888888 });
        
        // Create mesh
        const mesh = new THREE.Mesh(geometry, material);
        mesh.name = layerData.name;
        mesh.userData = {
            layerIndex: layerIndex,
            materialType: layerData.materialType,
            region: layerData.region,
            faceIds: layerData.faceIds  // Your existing face ID system
        };
        
        // Add to scene and tracking
        this.scene.add(mesh);
        layerData.faceIds.forEach(faceId => {
            this.meshObjects.set(faceId, mesh);
        });
    }
    
    /**
     * Setup WebSocket for real-time updates from your backend
     */
    setupWebSocket(deviceId) {
        if (this.socket) {
            this.socket.close();
        }
        
        this.socket = new WebSocket(`ws://localhost:8080/api/devices/${deviceId}/updates`);
        
        this.socket.onopen = () => {
            console.log('WebSocket connected for real-time updates');
        };
        
        this.socket.onmessage = (event) => {
            const update = JSON.parse(event.data);
            this.handleGeometryUpdate(update);
        };
        
        this.socket.onerror = (error) => {
            console.error('WebSocket error:', error);
        };
        
        this.socket.onclose = () => {
            console.log('WebSocket disconnected');
        };
    }
    
    /**
     * Handle incremental geometry updates (uses your existing face ID system)
     */
    handleGeometryUpdate(update) {
        console.log('Received geometry update:', update.type);
        
        switch (update.type) {
            case 'layer_added':
                this.handleLayerAdded(update.data);
                break;
                
            case 'layer_modified':
                this.handleLayerModified(update.data);
                break;
                
            case 'layer_removed':
                this.handleLayerRemoved(update.data);
                break;
                
            case 'mesh_refined':
                this.handleMeshRefined(update.data);
                break;
                
            case 'full_update':
                this.loadMeshData(update.data);
                break;
        }
        
        this.render();
    }
    
    handleLayerAdded(layerData) {
        this.createLayerMesh(layerData, this.scene.children.length);
    }
    
    handleLayerModified(updateData) {
        // Remove old geometry for modified faces
        updateData.modifiedFaceIds.forEach(faceId => {
            const mesh = this.meshObjects.get(faceId);
            if (mesh) {
                this.scene.remove(mesh);
                this.meshObjects.delete(faceId);
            }
        });
        
        // Add new geometry
        updateData.newMeshData.layers.forEach(layerData => {
            this.createLayerMesh(layerData, -1);
        });
    }
    
    handleLayerRemoved(layerData) {
        layerData.faceIds.forEach(faceId => {
            const mesh = this.meshObjects.get(faceId);
            if (mesh) {
                this.scene.remove(mesh);
                this.meshObjects.delete(faceId);
            }
        });
    }
    
    handleMeshRefined(meshData) {
        // Full mesh reload for now (could be optimized for incremental)
        this.clearGeometry();
        this.loadMeshData(meshData);
    }
    
    clearGeometry() {
        // Remove all mesh objects
        this.meshObjects.forEach((mesh, faceId) => {
            this.scene.remove(mesh);
            if (mesh.geometry) mesh.geometry.dispose();
        });
        this.meshObjects.clear();
        
        // Clear materials
        this.materials.forEach(material => material.dispose());
        this.materials.clear();
    }
    
    fitCameraToGeometry() {
        if (this.meshObjects.size === 0) return;
        
        // Calculate bounding box
        const bbox = new THREE.Box3();
        this.meshObjects.forEach(mesh => {
            bbox.expandByObject(mesh);
        });
        
        // Position camera to view entire geometry
        const center = bbox.getCenter(new THREE.Vector3());
        const size = bbox.getSize(new THREE.Vector3());
        const maxDim = Math.max(size.x, size.y, size.z);
        
        this.camera.position.copy(center);
        this.camera.position.add(new THREE.Vector3(maxDim, maxDim, maxDim));
        this.camera.lookAt(center);
        this.controls.target.copy(center);
        this.controls.update();
    }
    
    onWindowResize() {
        const container = document.getElementById(this.containerId);
        this.camera.aspect = container.clientWidth / container.clientHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(container.clientWidth, container.clientHeight);
    }
    
    animate() {
        requestAnimationFrame(() => this.animate());
        this.controls.update();
        this.render();
    }
    
    render() {
        this.renderer.render(this.scene, this.camera);
    }
    
    // Cleanup
    dispose() {
        if (this.socket) {
            this.socket.close();
        }
        this.clearGeometry();
        this.renderer.dispose();
    }
}

/**
 * Command interface for sending geometry operations to your existing backend
 */
class SemiconductorDeviceController {
    constructor(apiBaseUrl = 'http://localhost:8080/api') {
        this.apiBaseUrl = apiBaseUrl;
    }
    
    // Device management (calls your existing SemiconductorDevice methods)
    async createDevice(name) {
        const response = await fetch(`${this.apiBaseUrl}/devices`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name })
        });
        return response.json();
    }
    
    async getDevice(deviceId) {
        const response = await fetch(`${this.apiBaseUrl}/devices/${deviceId}`);
        return response.json();
    }
    
    // Layer operations (calls your existing addLayer, removeLayer methods)
    async addLayer(deviceId, layerConfig) {
        const response = await fetch(`${this.apiBaseUrl}/devices/${deviceId}/layers`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(layerConfig)
        });
        return response.json();
    }
    
    async removeLayer(deviceId, layerName) {
        const response = await fetch(`${this.apiBaseUrl}/devices/${deviceId}/layers/${layerName}`, {
            method: 'DELETE'
        });
        return response.json();
    }
    
    // Geometry operations (calls your existing GeometryBuilder methods)
    async performBooleanOperation(deviceId, operation) {
        const response = await fetch(`${this.apiBaseUrl}/devices/${deviceId}/operations/boolean`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(operation)
        });
        return response.json();
    }
    
    // Mesh operations (calls your existing mesh generation methods)
    async generateMesh(deviceId, meshConfig) {
        const response = await fetch(`${this.apiBaseUrl}/devices/${deviceId}/mesh/generate`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(meshConfig)
        });
        return response.json();
    }
    
    async refineMesh(deviceId, refinementConfig) {
        const response = await fetch(`${this.apiBaseUrl}/devices/${deviceId}/mesh/refine`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(refinementConfig)
        });
        return response.json();
    }
    
    // Device templates (calls your existing template methods)
    async createMOSFET(deviceId, mosfetConfig) {
        const response = await fetch(`${this.apiBaseUrl}/devices/${deviceId}/templates/mosfet`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(mosfetConfig)
        });
        return response.json();
    }
    
    // Export operations (calls your existing export methods)
    async exportGeometry(deviceId, format) {
        const response = await fetch(`${this.apiBaseUrl}/devices/${deviceId}/export/geometry/${format}`);
        return response.blob();
    }
    
    async exportMesh(deviceId, format) {
        const response = await fetch(`${this.apiBaseUrl}/devices/${deviceId}/export/mesh/${format}`);
        return response.blob();
    }
}

// Usage example:
// const viewer = new GeometryViewer('geometry-container');
// const controller = new SemiconductorDeviceController();
//
// // Create device using your existing backend
// const device = await controller.createDevice('Test MOSFET');
// 
// // Add layers using your existing methods
// await controller.addLayer(device.id, {
//     type: 'box',
//     material: 'Silicon',
//     region: 'Substrate',
//     dimensions: { length: 100e-6, width: 100e-6, height: 50e-6 }
// });
//
// // Display in 3D viewer
// await viewer.loadDevice(device.id);
