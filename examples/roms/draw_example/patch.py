import os, re
path = "examples/rom/draw/olive.c"
with open(path, "r") as f: code = f.read()

# 1. Altera tudo que é 32 bits (ARGB) para 8 bits (Index)
code = code.replace("uint32_t", "uint8_t")

# 2. Simplifica a mistura de cores para usar cores fixas (transparência=0)
code = re.sub(r"OLIVECDEF void olivec_blend_color\(uint8_t \*c1, uint8_t c2\)\s*\{.*?\}", 
              "OLIVECDEF void olivec_blend_color(uint8_t *c1, uint8_t c2)\n{\n    if (c2 != 0) *c1 = c2;\n}", 
              code, flags=re.DOTALL)

code = re.sub(r"OLIVECDEF uint8_t mix_colors2\(uint8_t c1, uint8_t c2, int u1, int det\)\s*\{.*?\}", 
              "OLIVECDEF uint8_t mix_colors2(uint8_t c1, uint8_t c2, int u1, int det)\n{\n    return (u1 * 2 > det) ? c1 : c2;\n}", 
              code, flags=re.DOTALL)

code = re.sub(r"OLIVECDEF uint8_t mix_colors3\(uint8_t c1, uint8_t c2, uint8_t c3, int u1, int u2, int det\)\s*\{.*?\}", 
              "OLIVECDEF uint8_t mix_colors3(uint8_t c1, uint8_t c2, uint8_t c3, int u1, int u2, int det)\n{\n    int u3 = det - u1 - u2;\n    if (u1 >= u2 && u1 >= u3) return c1;\n    if (u2 >= u1 && u2 >= u3) return c2;\n    return c3;\n}", 
              code, flags=re.DOTALL)

# 3. Conserta o cálculo alpha original dos círculos que vai quebrar no 8-bit
circle_aa = """            int count = 0;
            for (int sox = 0; sox < OLIVEC_AA_RES; ++sox) {
                for (int soy = 0; soy < OLIVEC_AA_RES; ++soy) {
                    int res1 = (OLIVEC_AA_RES + 1);
                    int dx = (x*res1*2 + 2 + sox*2 - res1*cx*2 - res1);
                    int dy = (y*res1*2 + 2 + soy*2 - res1*cy*2 - res1);
                    if (dx*dx + dy*dy <= res1*res1*r*r*2*2) count += 1;
                }
            }
            if (count > 0) olivec_blend_color(&OLIVEC_PIXEL(oc, x, y), color);"""

code = re.sub(r"\s*int count = 0;.*?olivec_blend_color\(&OLIVEC_PIXEL\(oc, x, y\), updated_color\);", 
              "\n" + circle_aa, 
              code, flags=re.DOTALL)

with open(path, "w") as f: f.write(code)
print("olive.c convertido para 8-bit!")
