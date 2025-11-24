# Proyecto - Paralela
Este proyecto desarrolla una simulación ecológica donde conejos, zorros y rocas interactúan dentro de un entorno discreto. 
La dinámica del sistema se basa en reglas que gobiernan movimiento, reproducción, alimentación y supervivencia, permitiendo observar comportamientos emergentes propios de modelos depredador–presa.

La implementación está construida en dos variantes: una versión secuencial y una versión paralela mediante OpenMP, conservando en ambas la misma lógica y semántica de evolución. El cálculo de los planes de movimiento se paraleliza de forma segura y determinista, garantizando que la simulación paralela replique fielmente los resultados de la ejecución secuencial sin introducir condiciones de carrera.

El proyecto concluye mostrando el estado final del ecosistema, las estadísticas recopiladas y un análisis de rendimiento que incluye tiempos de ejecución, speedup y una estimación basada en la Ley de Amdahl. Con esto se logra no solo modelar el comportamiento ecológico, sino también evaluar el impacto real de la paralelización en la eficiencia computacional.

# Instrucciones de compilacion:
` g++ -std=c++17 -O2 -Wall -fopenmp <nombre_programa> -o <nombre_ejecutable>`

# Comando para ejecutar:
`./<nombre_ejecutable>`
