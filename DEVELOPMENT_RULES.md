# Regras e Padrões de Desenvolvimento

## Visão Geral

Este documento define as regras, padrões e convenções de desenvolvimento para o projeto Satisfaction Hub.

## Linguagem e Padrões

### Linguagem Principal
- **C++17** ou superior
- Use recursos modernos do C++ quando apropriado (RAII, smart pointers, etc.)
- Evite C-style quando possível, mas mantenha compatibilidade com APIs C do ESP-IDF

### Convenções de Nomenclatura

#### Classes
- **PascalCase**: `DisplayDriver`, `Xpt2046Bitbang`
- Use substantivos descritivos
- Evite abreviações desnecessárias

#### Funções e Métodos
- **camelCase**: `init()`, `getTouch()`, `lvgl_display()`
- Use verbos para ações: `init`, `create`, `update`, `destroy`
- Use substantivos para getters: `getTouch()`, `lvgl_display()`

#### Variáveis
- **snake_case**: `panel_handle_`, `touch_controller_`, `selected_rating`
- Use sufixo `_` para membros de classe privados
- Use prefixo `m_` ou sufixo `_` para membros de classe (escolhemos `_`)

#### Constantes
- **UPPER_SNAKE_CASE**: `LCD_H_RES`, `PIN_NUM_MOSI`, `TOUCH_CALIB`
- Use `constexpr` quando possível
- Agrupe constantes relacionadas em namespaces ou structs

#### Namespaces
- Use namespaces para organizar código: `ui::init()`, `ui::update()`
- Evite namespaces aninhados profundos

### Estrutura de Arquivos

```
satisfaction-hub/
├── components/           # Componentes customizados
│   ├── component_name/
│   │   ├── CMakeLists.txt
│   │   ├── component.cpp
│   │   └── include/
│   │       └── component.hpp
├── main/                # Aplicação principal
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.cpp
├── docs/                # Documentação adicional (opcional)
└── README.md
```

## Componentes

### Criação de Componentes

1. **Cada componente deve ter**:
   - `CMakeLists.txt` com `idf_component_register`
   - Diretório `include/` com headers públicos
   - Arquivos fonte na raiz do componente

2. **Headers (`*.hpp`)**:
   - Use `#pragma once` para include guards
   - Documente APIs públicas com comentários Doxygen-style
   - Mantenha headers limpos e focados

3. **Implementação (`*.cpp`)**:
   - Implemente toda lógica complexa aqui
   - Use namespaces anônimos para funções auxiliares privadas

### Dependências entre Componentes

- Declare dependências explicitamente em `CMakeLists.txt` via `REQUIRES`
- Use `REQUIRES` para componentes ESP-IDF: `esp_driver_spi`, `esp_lcd`
- Use `REQUIRES` para componentes gerenciados: `espressif__esp_lcd_ili9341`
- Use `REQUIRES` para componentes locais: `display_driver`, `ui_driver`

## Gerenciamento de Memória

### Alocação Dinâmica
- **Prefira stack allocation** quando possível
- Use `heap_caps_malloc()` para buffers DMA-capable
- **Sempre libere memória** alocada dinamicamente
- Use RAII quando possível

### Buffers LVGL
- Aloque buffers LVGL em DMA-capable memory
- Use `esp_heap_caps_malloc()` com `MALLOC_CAP_DMA`
- Verifique retorno de alocação

### Stack Size
- Monitore uso de stack
- Aumente `CONFIG_ESP_MAIN_TASK_STACK_SIZE` se necessário
- Use `uxTaskGetStackHighWaterMark()` para debug

## Thread Safety

### LVGL
- **Sempre** use `lvgl_port_lock()` antes de operações LVGL
- **Sempre** use `lvgl_port_unlock()` após operações LVGL
- Use `portMAX_DELAY` para locks bloqueantes
- Nunca chame funções LVGL sem lock

```cpp
lvgl_port_lock(portMAX_DELAY);
// ... operações LVGL ...
lvgl_port_unlock();
```

### FreeRTOS
- Use semáforos/mutexes para recursos compartilhados
- Evite operações bloqueantes em callbacks de interrupção
- Use queues para comunicação entre tasks

## Tratamento de Erros

### Padrão ESP-IDF
- Use `esp_err_t` para retornos de erro
- Use `ESP_ERROR_CHECK()` para erros críticos
- Use `ESP_GOTO_ON_ERROR()` para cleanup em caso de erro
- Logue erros com `ESP_LOGE()` antes de retornar

### Validação de Parâmetros
- Valide ponteiros antes de usar: `if (ptr == nullptr) return ESP_ERR_INVALID_ARG;`
- Valide valores de entrada em funções públicas
- Retorne `ESP_ERR_INVALID_ARG` para parâmetros inválidos

## Logging

### Níveis de Log
- **ESP_LOGE**: Erros críticos que impedem funcionamento
- **ESP_LOGW**: Avisos sobre condições anômalas
- **ESP_LOGI**: Informações importantes sobre estado do sistema
- **ESP_LOGD**: Debug detalhado (desabilitado em produção)
- **ESP_LOGV**: Verbose (muito detalhado, raramente usado)

### Tags
- Use tags descritivas: `"DisplayDriver"`, `"UI"`, `"XPT2046_BB"`
- Mantenha tags consistentes dentro de um componente
- Use `constexpr char TAG[] = "ComponentName";`

## Configuração

### sdkconfig
- Documente mudanças importantes em `sdkconfig`
- Não commite `sdkconfig.old` ou arquivos temporários
- Use `idf.py menuconfig` para alterações

### Configurações Críticas
- `CONFIG_ESP_MAIN_TASK_STACK_SIZE`: 9216 bytes (aumentado para LVGL)
- `CONFIG_XPT2046_INTERRUPT_MODE`: Habilitado para touch
- Configurações LVGL via `lv_conf.h` (gerado automaticamente)

## Versionamento

### Git
- Use commits descritivos: `"feat: adiciona suporte a touch screen"`
- Use prefixos: `feat:`, `fix:`, `docs:`, `refactor:`, `test:`
- Commite frequentemente, mas mantenha commits lógicos

### Dependências
- Especifique versões em `idf_component.yml`
- Use caret (`^`) para atualizações compatíveis: `^2.6.3`
- Documente atualizações de dependências

## Testes

### Estrutura
- Crie testes unitários quando apropriado
- Teste componentes isoladamente quando possível
- Use `idf.py test` para executar testes

### Debug
- Use `ESP_LOGD()` para debug detalhado
- Use breakpoints no debugger quando necessário
- Monitore stack usage com `uxTaskGetStackHighWaterMark()`

## Documentação

### Código
- Documente funções públicas com comentários
- Explique "por quê", não apenas "o quê"
- Mantenha comentários atualizados com o código

### README
- Mantenha `README.md` atualizado
- Documente mudanças significativas
- Inclua exemplos de uso quando apropriado

## Performance

### Otimizações
- Evite otimizações prematuras
- Meça performance antes de otimizar
- Use `idf.py size-components` para analisar tamanho

### LVGL
- Minimize redraws desnecessários
- Use `lv_obj_invalidate()` apenas quando necessário
- Configure buffers adequados para performance

## Segurança

### Validação de Entrada
- Valide todos os inputs de usuário
- Valide dados de sensores antes de usar
- Use bounds checking em arrays

### Comunicação
- Se implementar WiFi/BLE, use criptografia
- Valide dados recebidos via rede
- Não exponha credenciais no código

## Checklist de Pull Request

Antes de submeter código, verifique:

- [ ] Código compila sem warnings
- [ ] Segue convenções de nomenclatura
- [ ] Tem tratamento de erros adequado
- [ ] Usa `lvgl_port_lock/unlock` corretamente
- [ ] Documentação atualizada
- [ ] Testado no hardware real
- [ ] Logs apropriados adicionados
- [ ] Sem memory leaks (verificado com valgrind/heap tracing)

## Recursos

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [LVGL Documentation](https://docs.lvgl.io/)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)




