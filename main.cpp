#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <omp.h>
#include <random>

// Struct in stile C++20 per gestire i colori
struct Color {
    uint8_t r{0}, g{0}, b{0};
    float_t a{0.0f};
};

// Definizione del cerchio
struct Circle {
    float x;      // Centro X
    float y;      // Centro Y
    float radius; // Raggio
    Color color;  // Colore del cerchio
};

int main() {
    // Configurazione dell'immagine di output
    constexpr int width = 800;
    constexpr int height = 800;

    std::random_device rd;
    unsigned int idUnico = rd();
    // Generatore Rand da seed
    std::mt19937 gen(idUnico);

    // Definiamo una distribuzione uniforme (es. numeri interi da 1 a 100)
    std::uniform_int_distribution<int> rgbDistrib(0, 255);
    std::uniform_int_distribution<int> sizeDistrib(0, 100);

    std::cout << "\n";
    // Vettore lineare per contenere tutti i pixel (Buffer d'immagine)
    std::vector<Color> imageBuffer(width * height, Color{.r = 0, .g = 0, .b = 0}); // Sfondo grigio scuro


    // Creiamo una lista di cerchi usando i designated initializers di C++20
    std::vector<Circle> circles = std::vector<Circle>{};
    for (int i = 0; i < 1000; i++) {
        Circle circle = {
            static_cast<float>(sizeDistrib(gen)* 8),
            static_cast<float>(sizeDistrib(gen) * 8),
            static_cast<float>(sizeDistrib(gen)),
            static_cast<uint8_t>(rgbDistrib(gen)),
            static_cast<uint8_t>(rgbDistrib(gen)),
            static_cast<uint8_t>(rgbDistrib(gen)),
            (static_cast<float_t>(sizeDistrib(gen)) / 100.0f),
        };
        circles.push_back(circle);
    }

    std::cout << "Rendering in corso con OpenMP..." << std::endl;
    double startTime = omp_get_wtime();

    // PARALLELIZZAZIONE CON OPENMP
    // Usiamo 'collapse(2)' per unire i due cicli for in un unico grande loop parallelo distribuito sui core dell'M1
    #pragma omp parallel for collapse(2) schedule(dynamic)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {

            // Controlliamo i cerchi dal fondo in avanti (l'ultimo disegnato sta sopra)
            for (const auto& circle : circles) {
                // Equazione del cerchio: (x - cx)^2 + (y - cy)^2 <= r^2
                float dx = static_cast<float>(x) - circle.x;
                float dy = static_cast<float>(y) - circle.y;

                if ((dx * dx) + (dy * dy) <= (circle.radius * circle.radius)) {
                    // Trovato impatto: coloriamo il pixel nel buffer lineare
                    // Prendiamo il colore che c'è già in quel pixel (Destinazione)
                    Color currentBackground = imageBuffer[y * width + x];
                    float a = circle.color.a;

                    // Formula Alpha Blending applicata a R, G, B
                    float newR = (static_cast<float>(circle.color.r) * a) + (static_cast<float>(currentBackground.r) * (1.0f - a));
                    float newG = (static_cast<float>(circle.color.g) * a) + (static_cast<float>(currentBackground.g) * (1.0f - a));
                    float newB = (static_cast<float>(circle.color.b) * a) + (static_cast<float>(currentBackground.b) * (1.0f - a));

                    imageBuffer[y * width + x] = Color{
                        .r = static_cast<uint8_t>(newR),
                        .g = static_cast<uint8_t>(newG),
                        .b = static_cast<uint8_t>(newB),
                        .a = 1.0f // Lo sfondo finale rimane opaco
                    };
                }
            }

        }
    }

    double endTime = omp_get_wtime();
    std::cout << "Rendering completato in: " << (endTime - startTime) << " secondi." << std::endl;

    std::string nomeFile = std::format("render_{}.ppm", idUnico);
    // Salviamo l'immagine su file in formato PPM
    std::ofstream outFile(nomeFile);
    if (!outFile) {
        std::cerr << "Errore nella creazione del file di output!" << std::endl;
        return 1;
    }

    // Header del file PPM
    outFile << "P3\n" << width << " " << height << "\n255\n";

    // Scrittura dei pixel su file
    for (const auto& pixel : imageBuffer) {
        outFile << static_cast<int>(pixel.r) << " "
                << static_cast<int>(pixel.g) << " "
                << static_cast<int>(pixel.b) << "\n";
    }

    std::cout << "Immagine salvata come " << nomeFile << std::endl;
    return 0;
}
