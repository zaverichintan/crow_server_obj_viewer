import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

// Global state
let scene, camera, renderer, controls;
let mesh, wireMesh, pivot;
let triangleCount = 0;

// Initialize the application
async function init() {
    const viewport = document.getElementById('viewport');

    // Set up Three.js scene
    scene = new THREE.Scene();
    scene.background = new THREE.Color(0x1a1a2e);

    // Set up camera
    const width = viewport.clientWidth;
    const height = viewport.clientHeight;
    camera = new THREE.PerspectiveCamera(60, width / height, 0.1, 1000);
    camera.position.set(3, 3, 3);
    camera.lookAt(0, 0, 0);

    // Set up renderer
    renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.setSize(width, height);
    viewport.appendChild(renderer.domElement);

    // Add lighting
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.4);
    scene.add(ambientLight);

    const dirLight = new THREE.DirectionalLight(0xffffff, 1.0);
    dirLight.position.set(5, 10, 7.5);
    scene.add(dirLight);

    // Set up OrbitControls
    controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
    controls.autoRotate = false;

    try {
        // Create empty pivot that will hold meshes
        pivot = new THREE.Object3D();
        scene.add(pivot);

        // Create material for mesh
        const material = new THREE.MeshPhongMaterial({
            color: 0x4488ff,
            side: THREE.DoubleSide,
            flatShading: true,
            wireframe: false
        });

        // Create wireframe material
        const wireMaterial = new THREE.MeshBasicMaterial({
            color: 0xffffff,
            wireframe: true,
            transparent: true,
            opacity: 0.3
        });

        // Create placeholder geometries
        const placeholderGeo = new THREE.BoxGeometry();
        mesh = new THREE.Mesh(placeholderGeo, material);
        wireMesh = new THREE.Mesh(placeholderGeo, wireMaterial);
        pivot.add(mesh);
        pivot.add(wireMesh);

        // Load model list and populate dropdown
        await loadModels();

        // Load default mesh
        await loadMesh("");

        // Set up UI controls
        setupControls();

        // Start render loop
        animate();

    } catch (error) {
        console.error('Error initializing:', error);
        document.getElementById('stats').innerHTML = `<p>❌ Error: ${error.message}</p>`;
    }
}

async function loadModels() {
    try {
        const response = await fetch('/api/models');
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }

        const data = await response.json();
        const select = document.getElementById('model-select');

        // Add each model as an option
        for (const model of data.models) {
            const option = document.createElement('option');
            option.value = model;
            option.textContent = model;
            select.appendChild(option);
        }

        // Add change event listener
        select.addEventListener('change', (e) => {
            loadMesh(e.target.value);
        });

    } catch (error) {
        console.error('Error loading models:', error);
    }
}

async function loadMesh(modelName) {
    try {
        console.log('loadMesh called with:', modelName);
        console.log('pivot exists:', !!pivot);
        console.log('mesh exists:', !!mesh);
        console.log('wireMesh exists:', !!wireMesh);

        // Build URL with query parameter if model is specified
        const url = modelName ? `/api/triangles?model=${encodeURIComponent(modelName)}` : '/api/triangles';
        const response = await fetch(url);
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }

        const data = await response.json();
        if (data.error) {
            throw new Error(data.error);
        }

        triangleCount = data.triangle_count;
        console.log('Got triangles:', triangleCount);

        // Build new geometry
        const geometry = buildGeometry(data.triangles);
        console.log('Built geometry');

        // Remove old meshes from pivot if they exist
        if (mesh && pivot) {
            pivot.remove(mesh);
            if (mesh.geometry) mesh.geometry.dispose();
            if (mesh.material) mesh.material.dispose();
        }
        if (wireMesh && pivot) {
            pivot.remove(wireMesh);
            if (wireMesh.geometry) wireMesh.geometry.dispose();
            if (wireMesh.material) wireMesh.material.dispose();
        }

        // Create new mesh with material
        const material = new THREE.MeshPhongMaterial({
            color: 0x4488ff,
            side: THREE.DoubleSide,
            flatShading: true,
            wireframe: false
        });

        mesh = new THREE.Mesh(geometry, material);
        console.log('Created mesh:', !!mesh);

        // Create wireframe overlay
        const wireMaterial = new THREE.MeshBasicMaterial({
            color: 0xffffff,
            wireframe: true,
            transparent: true,
            opacity: 0.3
        });

        wireMesh = new THREE.Mesh(geometry, wireMaterial);
        console.log('Created wireMesh:', !!wireMesh);

        // Add new meshes to pivot
        if (pivot) {
            pivot.add(mesh);
            pivot.add(wireMesh);
            console.log('Added meshes to pivot');
        } else {
            console.error('pivot is undefined!');
        }

        // Reset transforms
        try {
            resetControls();
        } catch (e) {
            console.error('Error in resetControls:', e);
        }

        // Re-center camera on new geometry
        try {
            recenterCamera(geometry);
        } catch (e) {
            console.error('Error in recenterCamera:', e);
        }

        // Update stats
        updateStats();

    } catch (error) {
        console.error('Error loading mesh:', error);
        document.getElementById('stats').innerHTML = `<p>❌ Error loading mesh: ${error.message}</p>`;
    }
}

function resetControls() {
    if (!pivot) {
        console.error('resetControls: pivot is undefined');
        return;
    }

    // Reset rotation sliders
    document.getElementById('rotateX').value = 0;
    document.getElementById('rotateY').value = 0;
    document.getElementById('rotateZ').value = 0;
    document.getElementById('rotateX-val').textContent = 0;
    document.getElementById('rotateY-val').textContent = 0;
    document.getElementById('rotateZ-val').textContent = 0;

    // Reset translation inputs
    document.getElementById('translateX').value = 0;
    document.getElementById('translateY').value = 0;
    document.getElementById('translateZ').value = 0;

    // Reset scale
    document.getElementById('scale').value = 1;
    document.getElementById('scale-val').textContent = '1.0';

    // Apply resets to pivot
    pivot.rotation.set(0, 0, 0);
    pivot.position.set(0, 0, 0);
    pivot.scale.set(1, 1, 1);
}

function recenterCamera(geometry) {
    if (!camera || !controls) {
        console.error('recenterCamera: camera or controls is undefined');
        return;
    }

    // Compute bounding box
    geometry.computeBoundingBox();
    const boundingBox = geometry.boundingBox;
    const center = new THREE.Vector3();
    boundingBox.getCenter(center);

    const size = new THREE.Vector3();
    boundingBox.getSize(size);
    const maxDim = Math.max(size.x, size.y, size.z);
    const fov = camera.fov * (Math.PI / 180);
    let cameraDistance = maxDim / 2 / Math.tan(fov / 2);
    cameraDistance *= 2.5; // Add some padding

    // Position camera along the diagonal
    const direction = new THREE.Vector3(1, 1, 1).normalize();
    camera.position.copy(center).addScaledVector(direction, cameraDistance);
    camera.lookAt(center);
    controls.target.copy(center);
    controls.update();
}

function buildGeometry(triangles) {
    // Flatten triangle data into position array
    const positions = [];

    for (const tri of triangles) {
        // Add first vertex
        positions.push(tri.a.x, tri.a.y, tri.a.z);
        // Add second vertex
        positions.push(tri.b.x, tri.b.y, tri.b.z);
        // Add third vertex
        positions.push(tri.c.x, tri.c.y, tri.c.z);
    }

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(positions), 3));
    geometry.computeVertexNormals();

    return geometry;
}

function setupControls() {
    // Rotation controls
    const rotateXInput = document.getElementById('rotateX');
    const rotateYInput = document.getElementById('rotateY');
    const rotateZInput = document.getElementById('rotateZ');

    rotateXInput.addEventListener('input', (e) => {
        const val = parseFloat(e.target.value);
        pivot.rotation.x = val * (Math.PI / 180);
        document.getElementById('rotateX-val').textContent = val;
    });

    rotateYInput.addEventListener('input', (e) => {
        const val = parseFloat(e.target.value);
        pivot.rotation.y = val * (Math.PI / 180);
        document.getElementById('rotateY-val').textContent = val;
    });

    rotateZInput.addEventListener('input', (e) => {
        const val = parseFloat(e.target.value);
        pivot.rotation.z = val * (Math.PI / 180);
        document.getElementById('rotateZ-val').textContent = val;
    });

    // Translation controls
    const translateXInput = document.getElementById('translateX');
    const translateYInput = document.getElementById('translateY');
    const translateZInput = document.getElementById('translateZ');

    translateXInput.addEventListener('input', (e) => {
        pivot.position.x = parseFloat(e.target.value);
    });

    translateYInput.addEventListener('input', (e) => {
        pivot.position.y = parseFloat(e.target.value);
    });

    translateZInput.addEventListener('input', (e) => {
        pivot.position.z = parseFloat(e.target.value);
    });

    // Scale control
    const scaleInput = document.getElementById('scale');
    scaleInput.addEventListener('input', (e) => {
        const val = parseFloat(e.target.value);
        pivot.scale.set(val, val, val);
        document.getElementById('scale-val').textContent = val.toFixed(2);
    });
}

function updateStats() {
    document.getElementById('stats').innerHTML = `
        <p><strong>Mesh Stats:</strong></p>
        <p>Triangles: ${triangleCount}</p>
        <p>Vertices: ${triangleCount * 3}</p>
        <p style="margin-top: 10px; font-size: 12px; color: #666;">
            💡 <strong>Controls:</strong><br>
            • Drag mouse to rotate (camera)<br>
            • Use sliders to transform mesh<br>
            • Scroll to zoom
        </p>
    `;
}

function animate() {
    requestAnimationFrame(animate);
    controls.update();
    renderer.render(scene, camera);
}

// Handle window resize
window.addEventListener('resize', () => {
    const viewport = document.getElementById('viewport');
    const width = viewport.clientWidth;
    const height = viewport.clientHeight;

    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    renderer.setSize(width, height);
});

// Start the application when THREE.js is available
function waitForTHREE() {
    if (typeof THREE !== 'undefined') {
        init();
    } else {
        setTimeout(waitForTHREE, 100);
    }
}

window.addEventListener('load', waitForTHREE);
