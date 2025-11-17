# Como Adicionar Suporte a Acentos Portugueses

As fontes padrão do LVGL (Montserrat) só incluem caracteres ASCII básicos. Para suportar acentos do português brasileiro (á, é, í, ó, ú, ã, õ, ç, etc.), você precisa criar uma fonte customizada.

## Passo 1: Gerar Fonte Customizada

1. Acesse o LVGL Font Converter: https://lvgl.io/tools/fontconverter

2. Configure a fonte:
   - **Font file**: Escolha uma fonte TTF (recomendado: Montserrat ou Roboto)
   - **Size**: Escolha os tamanhos que você precisa (ex: 16, 20, 24, 26)
   - **BPP (Bits Per Pixel)**: 4 (para melhor qualidade)
   - **Subset**: Selecione "Custom range"
   - **Range**: Digite: `32-255` (inclui ASCII + Latin-1 Extended com todos os acentos)
   - Ou use: `32-126, 160-255` para incluir espaços não-quebráveis também

3. Clique em "Convert" e baixe os arquivos `.c` gerados

## Passo 2: Adicionar ao Projeto

1. Crie uma pasta `components/custom_fonts/` no projeto
2. Copie os arquivos `.c` gerados para essa pasta
3. Crie um `CMakeLists.txt` na pasta:

```cmake
idf_component_register(SRCS "lv_font_montserrat_16_latin1.c" 
                              "lv_font_montserrat_20_latin1.c"
                              "lv_font_montserrat_24_latin1.c"
                              "lv_font_montserrat_26_latin1.c"
                       INCLUDE_DIRS ".")
```

## Passo 3: Usar no Código

No arquivo `ui_driver.cpp`, adicione:

```cpp
// Declarar as fontes customizadas
LV_FONT_DECLARE(lv_font_montserrat_16_latin1);
LV_FONT_DECLARE(lv_font_montserrat_20_latin1);
LV_FONT_DECLARE(lv_font_montserrat_24_latin1);
LV_FONT_DECLARE(lv_font_montserrat_26_latin1);

// Usar as fontes customizadas
static const lv_font_t *TITLE_FONT = &lv_font_montserrat_24_latin1;
static const lv_font_t *TEXT_FONT = &lv_font_montserrat_20_latin1;
static const lv_font_t *CAPTION_FONT = &lv_font_montserrat_16_latin1;
```

## Caracteres Suportados (Latin-1 Extended)

A faixa 32-255 inclui:
- ASCII básico (32-126)
- Caracteres acentuados: á, é, í, ó, ú, ã, õ, ç, etc.
- Símbolos especiais: ©, ®, °, etc.

## Nota sobre Tamanho

Fontes com suporte a Latin-1 Extended serão maiores que as fontes ASCII básicas.
Para economizar espaço, você pode gerar apenas os caracteres que realmente precisa.

