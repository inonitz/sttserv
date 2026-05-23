import os
import onnx
import tvm
import tvm.relax as relax
import tvm.tirx as tir
from tvm.relax.frontend.onnx import from_onnx

# 1. Path and Target
base_path = "../../dependencies/models/sherpa-onnx/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8/"
target = tvm.target.Target({"kind": "vulkan", "from_device": 0})

# 2. Symbol Registry
# TVM Relax requires dynamic dimensions to be tirx.Var objects with dtype int64.
# We store them here to reuse them (e.g., if multiple inputs share a 'Dynamic' axis).
symbol_registry = {}


def get_symbolic_shape(onnx_shape):
    """
    Converts ONNX shape list [1, 128, 'Dynamic'] 
    into TVM shape tuple (1, 128, tirx.Var)
    """
    tvm_shape = []
    for i, dim in enumerate(onnx_shape):
        if dim == 'Dynamic' or dim <= 0:
            # Create a unique name for this dynamic dimension if not seen
            symbol_name = f"dim_{i}" 
            if symbol_name not in symbol_registry:
                symbol_registry[symbol_name] = tir.Var(symbol_name, "int64")
            tvm_shape.append(symbol_registry[symbol_name])
        else:
            tvm_shape.append(int(dim))
    return tuple(tvm_shape)


# 3. Model Files to Process
parts = ['encoder.int8.onnx', 'decoder.int8.onnx', 'joiner.int8.onnx']


for part in parts:
    onnx_path = os.path.join(base_path, part)
    if not os.path.exists(onnx_path):
        print(f"Skipping {part}: File not found.")
        continue

    print(f"\n--- Processing {part} ---")
    model = onnx.load(onnx_path)
    
    # DYNAMIC INSPECTION: Build the shape_dict automatically
    shape_dict = {}
    print("Detected Inputs:")
    for input_node in model.graph.input:
        # Get raw dimensions from ONNX
        raw_shape = [dim.dim_value if dim.dim_value > 0 else 'Dynamic' 
            for dim in input_node.type.tensor_type.shape.dim
        ]
        
        # Convert to TVM-compatible (Int64 TIRX Vars for Dynamic)
        tvm_shape = get_symbolic_shape(raw_shape)
        shape_dict[input_node.name] = tvm_shape
        
        print(f"  > Name: '{input_node.name}' | ONNX Shape: {raw_shape} | TVM Shape: {tvm_shape}")

    # 4. CONVERT TO RELAX
    print(f"Converting {part} to Relax IR...")
    try:
        mod = from_onnx(model, shape_dict=shape_dict)
    except Exception as e:
        print(f"Conversion failed for {part}: {e}")
        continue

    # 5. BUILD FOR VULKAN
    print(f"Building Vulkan kernels...")
    try:
        with target:
            # Legalize and optimize
            mod = relax.pipeline.get_pipeline("default_build")(mod)
            ex = relax.build(mod, target=target)
        
        # 6. EXPORT
        out_name = f"parakeet_{part.replace('.onnx', '')}_vulkan.so"
        ex.export_library(out_name)
        print(f"SUCCESS: Exported {out_name}")
    except Exception as e:
        print(f"Build failed for {part}: {e}")

print("\nDynamic compilation process finished.")