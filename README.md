# Controle de Estoque e Vendas

Aplicação TUI para Windows escrita em C. O sistema cadastra produtos, controla vendas, gera relatórios e salva os dados em arquivos UTF-16LE de largura fixa.

## Criadores

- Rai Charles Ramos
- Ellen Leticia Correia
- Gabriela Dias
- Giovanna Borba
- João Arthur
- Matheus Reis
- Nathan Victor
- Samuel Lopes

### UNASP - CENTRO ADVENTISTA DE SÃO PAULO

## Arquitetura

| Módulo | Responsabilidade |
|---|---|
| `main.c` | Fluxo da aplicação, menus, entradas, confirmações e mensagens ao usuário. |
| `produto.c/.h` | Estrutura `Produto` e validações locais dos campos. |
| `estoque.c/.h` | Regras de negócio, buscas, filtros, vendas e histórico. |
| `arquivo.c/.h` | Integração entre o estoque e os dois arquivos persistidos. |
| `arq16.c/.h` | Leitura e escrita física de registros UTF-16LE fixos. |
| `arq16_csv.c/.h` | Gerenciamento tabular dos registros formatados. |
| `str16.c/.h` | Utilitários para strings UTF-16. |
| `tui16.c/.h` | Backend da interface responsiva para terminal Windows. |

## Ideia central

O estoque mantém duas visões dos mesmos objetos `Produto`:

- `por_codigo`: vetor de ponteiros ordenado por código, usado na busca binária `O(log n)`;
- `cadastro`: vetor de ponteiros na ordem de inserção, usado na busca linear por nome `O(n)`.

Não há cópia duplicada dos produtos: os dois vetores apontam para os mesmos objetos.

## Recursos

- cadastrar, editar e remover produtos;
- buscar produto por código;
- buscar produtos por trecho do nome;
- registrar venda com validação de saldo;
- listar produtos ordenados por código;
- filtrar por categoria;
- gerar relatório de estoque baixo com limite configurável;
- mostrar menor e maior preço;
- consultar histórico de vendas;
- salvar e recarregar dados manualmente;
- salvar automaticamente após alterações confirmadas;
- navegar com setas ou atalhos numéricos de `1` a `9`;
- cancelar entradas e ações com `ESC`.

A navegação possui no máximo três camadas: menu principal → submenu → ação.

## Persistência

A aplicação usa dois arquivos:

- `produtos.csv16`: cadastro atual e quantidade atual de cada produto;
- `estoque.csv16`: linha de controle dos próximos IDs e histórico imutável das vendas.

A gravação usa arquivos temporários e backups antes de substituir os arquivos principais.

## Compilação

Requisitos: Windows Terminal e GCC para Windows, como MinGW-w64.

Pelo Prompt de Comando:

```bat
build.bat
```

Ou manualmente:

```bat
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 ^
  main.c produto.c estoque.c arquivo.c arq16_csv.c arq16.c str16.c tui16.c ^
  -o estoque.exe
```

## Execução

```bat
estoque.exe
```

Na primeira execução sem arquivos persistidos, o sistema começa vazio e cria os arquivos após a primeira gravação.

## Dados de exemplo

A pasta `dados_exemplo` contém um par consistente de arquivos UTF-16LE. Para iniciar com esses dados, copie ambos para a pasta do executável:

```bat
copy dados_exemplo\produtos.csv16 .
copy dados_exemplo\estoque.csv16 .
```

## Funções principais

- `estoque_cadastrar_produto`
- `estoque_atualizar_produto`
- `estoque_alterar_codigo`
- `estoque_remover_produto`
- `estoque_buscar_por_codigo_const`
- `estoque_buscar_por_nome`
- `estoque_registrar_venda`
- `arquivo_salvar_estoque`
- `arquivo_carregar_estoque`

A análise técnica completa está em `ANALISE_FINAL.md`.
