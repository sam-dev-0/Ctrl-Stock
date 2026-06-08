## Projeto 1 de Estrutura de Dados - Sistema de Estoque e Vendas

### Objetivo
Construir um sistema de linha de comando para controlar produtos, vendas e relatórios.
O projeto deve aplicar conteúdos de Notação Big-O, vetores ordenados e não ordenados.

### Escopo do sistema
- Cadastro de produtos com código, nome, categoria, preço e quantidade.
- Registro de vendas e atualização de estoque.
- Relatórios por categoria, menor/maior preço e produtos com baixo estoque.

### Requisitos funcionais (obrigatórios)
1. Cadastrar produto (código unico).
2. Editar produto (nome, preço, quantidade, categoria).
3. Remover produto pelo código.
4. Buscar produto por código (busca binária em vetor ordenado por código).
5. Buscar produtos por nome (busca linear em vetor não ordenado).
6. Registrar venda (reduz estoque, valida quantidade).
7. Listar produtos ordenados por código (vetor ordenado).
8. Listar produtos por categoria (filtro).
9. Relatório de estoque baixo (quantidade < limite configurável).
10. Salvar e carregar dados em arquivo (CSV ou JSON).

### Requisitos não funcionais (obrigatórios)
- Interface por terminal com menu claro.
- Dados persistidos em arquivo.
- Código organizado em modulos e funções.
- Tratamento de erros de entrada (número, texto, vazio).
- Complexidade explicada: justificar uso de busca linear e binária.

### Requisitos não funcionais (não obrigatórios, mas recomendados)
- Logs simples de operacoes (data/hora).
- Mensagens amigáveis ao usuario.
- Paginação simples na listagem.

### Regras de negócio
- Não permitir código duplicado (códigos unicos).
- Não permitir venda com estoque insuficiente (controle de estoque e venda).
- Preço deve ser positivo (controle e validação de dados).
- Quantidade não pode ser negativa (controle e validação de produtos).

### Estrutura sugerida (arquivos)
- main (menu e fluxo)
- produto (classe/estrutura e validacoes)
- estoque (operacoes de cadastro e busca)
- arquivos (salvar/carregar)

### Requisitos de dados
- Vetor não ordenado para cadastro inicial e buscas por nome.
- Vetor ordenado por código para busca binária.
- Operações devem manter o vetor ordenado ao inserir/remover.

### Versionamento e boas práticas (obrigatório)
- Usar Git com commits pequenos e mensagens claras.
- Criar repositório no GitHub e enviar o código.
- Escrever README com como executar e exemplos.
- Seguir convenção padronizada de programação.
- Usar funções pequenas e responsáveis.
- Evitar código duplicado.

### Entregáveis
- Código fonte completo.
- Arquivo de dados de exemplo.
- README com instruções.
- Relatório curto explicando escolhas de busca e ordenação.

### Critérios de avaliação
- Funciona sem erros.
- Atende todos os requisitos obrigatórios.
- Usa busca binaria em vetor ordenado corretamente.
- Persistencia de dados funcionando.
- Codigo limpo e organizado.
