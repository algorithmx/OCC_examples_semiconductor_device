PLAN:  “BRICK-0  –  De-coupled  Pipeline  +  Stable  ID”
===

# Goal  

Turn current monolithic "build shape → dump .vtk" into "build shape → **annotated** TopoDS → **serializer** → .vtp" while **keeping the file output identical** to what we ship today.

# Task list

1.  **Create a TopoDS-to-vtkPolyData translator class**  
    ```
    class PolyDataSerializer {
    public:
        vtkSmartPointer<vtkPolyData> convert(const TopoDS_Shape& shape);
    private:
        void visitFace(const TopoDS_Face& F, int id);
        int  nextId = 0;
    };
    ```
    - Every face gets a **serial integer id** (stored in `TDataStd_Integer` attribute on the face).  
    - The same id becomes the `<Piece index="id">` inside the eventual .vtp file.  
    → **Exit criterion 1**: you can round-trip  
    `TopoDS_Shape → convert → write .vtp → read .vtp → compare vertex count`  
    and obtain **bit-identical** coordinates to your current exporter.

2.  **Add a lightweight hash for every face**  
    ```
    uint64_t hashFace(const TopoDS_Face& F) {
        return hash(F.TShape()) ^ hash(F.Location().Transformation());
    }
    ```
    Store it in the same attribute bag.  
    → **Exit criterion 2**: two builds of the **same** model produce **identical** id+hash lists (deterministic).

3.  **Expose “serialize subset” API**  
    ```
    vtkSmartPointer<vtkPolyData>
    convertSubset(const std::vector<int>& faceIds);
    ```
    This will later be called by the diff engine.  
    → **Exit criterion 3**: you can delete one face in OCCT, then call  
    `convertSubset(remainingIds)` and the output polydata has **exactly** the triangles of the remaining faces.

4.  **Write a tiny CLI driver that still produces your old file**  
    ```
    int main(int argc, char** argv){
        TopoDS_Shape shape = BuildMyModel();   // your existing code
        PolyDataSerializer ser;
        auto pd  = ser.convert(shape);
        WriteVTP(pd, argv[1]);                 // identical file
    }
    ```
    → **Exit criterion 4**: ParaView opens the new file and shows **zero difference** (vertex count, cell count, bounds) compared with the file produced by your **old** exporter.

5.  **Unit-tests** (catch2 or gtest)  
    - test identical output for 3 reference models  
    - test hash stability across runs  
    - test `convertSubset` after random face removal  
    → **Exit criterion 5**: all tests green.

---
---

# Deliverables

- `PolyDataSerializer.{h,cpp}` – clean, deterministic, reusable  
- Stable face-ID + hash stored inside OCCT shape  
- Unit-test suite  
- No change in **user-visible** behaviour (same .vtp, same ParaView look)


